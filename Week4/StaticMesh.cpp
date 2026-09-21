#include "StaticMesh.h"
#include "Assets.h"
#include "Vector.h"
#include "TArray.h"
#include "FileManager.h"
#include "Material.h"
#include "FAssetManager.h"
#include "FLogManager.h"
#include "Archive.h"

namespace
{
	// BakedDir/PreferredStem.uasset이 이미 있으면(다른 원본이 쓰고 있는 이름이면) 번호를 붙여
	// 비어있는 경로를 찾는다. 재임포트 여부는 호출자가 이미 판단했으므로 여기선 신규 임포트만 다룬다.
	std::filesystem::path MakeUniqueBakedPath(const std::filesystem::path& BakedDir, const FString& PreferredStem)
	{
		std::filesystem::path Candidate = BakedDir / (std::string(PreferredStem.CStr()) + ".uasset");
		if (!std::filesystem::exists(Candidate))
		{
			return Candidate;
		}

		for (int32 Suffix = 1; ; ++Suffix)
		{
			std::filesystem::path Numbered = BakedDir / (std::string(PreferredStem.CStr()) + "_" + std::to_string(Suffix) + ".uasset");
			if (!std::filesystem::exists(Numbered))
			{
				return Numbered;
			}
		}
	}
}

FName FStaticMeshAssetLoader::Import(const std::filesystem::path& SourceObjPath, FFileManager& InFileManager)
{
	// 이미 같은 원본을 임포트한 적 있으면 그 .uasset 키를 그대로 재사용한다(=재임포트).
	FName ExistingKey;
	bool bIsReimport = AssetManager->FindAssetByImportPath(SourceObjPath, ExistingKey);

	std::filesystem::path BakedPath;
	if (bIsReimport)
	{
		BakedPath = std::filesystem::path(ExistingKey.ToString().CStr());
	}
	else
	{
		FString PreferredStem(SourceObjPath.stem().string());
		BakedPath = MakeUniqueBakedPath("Assets/Baked", PreferredStem);
	}

	FName BakedKey(FString(BakedPath.string()));

	// 이미 구워져 있고, 원본이 그 이후로 안 바뀌었으면 다시 파싱할 필요가 없다.
	bool bNeedsBake = true;
	if (std::filesystem::exists(BakedPath) && std::filesystem::exists(SourceObjPath))
	{
		bNeedsBake = std::filesystem::last_write_time(SourceObjPath) > std::filesystem::last_write_time(BakedPath);
	}

	if (bNeedsBake)
	{
		FFileAssetSource FileSource(InFileManager, SourceObjPath);

		FString FileContent = FileSource.ReadFileToString();
		FObjImporter Importer = FObjImporter{};
		FStaticMesh StaticMesh{};
		TArray<FString> MaterialFiles;

		StaticMesh.PathFileName = FString(FileSource.GetFilePath().string());
		Importer.LoadObjModel(FileContent, StaticMesh, MaterialFiles);

		// MaterialDatas[i]가 어느 mtl 파일(절대경로)에서 왔는지 같은 인덱스로 같이 들고 있는다.
		// 머티리얼의 진짜 출처는 obj가 아니라 mtl이라, 키를 만들 때 obj 디렉토리가 아니라 이걸 써야 한다.
		TArray<FMaterialData> MaterialDatas;
		TArray<std::filesystem::path> MaterialDataSourcePaths;
		for (FString filename : MaterialFiles)
		{
			std::filesystem::path ObjDirectory = FileSource.GetFilePath().parent_path();
			std::filesystem::path MtlPath = ObjDirectory / filename.CStr();
			FString MaterialFileContent = FileSource.GetFileManager().ReadFileToString(MtlPath);

			uint32 CountBefore = MaterialDatas.Num();
			Importer.ParseMtlFile(MaterialFileContent, MaterialDatas);
			for (uint32 i = CountBefore; i < MaterialDatas.Num(); ++i)
			{
				MaterialDataSourcePaths.Add(MtlPath);
			}
		}

		UStaticMesh* NewMesh = FObjectFactory::ConstructObject<UStaticMesh>(BakedKey);
		NewMesh->SetData(StaticMesh.Vertices, StaticMesh.Indices, StaticMesh.Sections);

		std::filesystem::path ObjDirectory = FileSource.GetFilePath().parent_path();
		TArray<FName> MaterialKeys;

		for (const FString& MaterialName : StaticMesh.Materials) {
			const FMaterialData* FoundMaterial = nullptr;
			const std::filesystem::path* FoundMtlPath = nullptr;

			for (uint32 i = 0; i < MaterialDatas.Num(); ++i) {
				if (MaterialDatas[i].Name == MaterialName) {
					FoundMaterial = &MaterialDatas[i];
					FoundMtlPath = &MaterialDataSourcePaths[i];
					break;
				}
			}

			// 머티리얼의 진짜 출처(=mtl 파일) + 이름으로 키를 만든다. mtl에 아예 없는 이름(오타 등)이면
			// 그런 파일 자체가 없으니 obj 디렉토리로 대신 유일성을 만든다.
			// 프로젝트 루트 기준 상대경로라 다른 컴퓨터에서도 같은 키가 나온다.
			std::filesystem::path MaterialKeyBase = FoundMtlPath ? *FoundMtlPath : ObjDirectory;
			FString MaterialAssetKey(FileSource.GetFileManager().MakeRelativeToRoot(MaterialKeyBase).string());
			MaterialAssetKey.Append("::");
			MaterialAssetKey.Append(MaterialName);
			FName MaterialAssetName(MaterialAssetKey);

			FMaterialAssetSource* MaterialSource = new FMaterialAssetSource(
				FileSource.GetFileManager(),
				ObjDirectory,
				FoundMaterial ? *FoundMaterial : FMaterialData{},
				FoundMaterial != nullptr
			);

			AssetManager->RegisterAsset<FMaterialAssetLoader>(MaterialAssetName, MaterialSource, Renderer, *AssetManager);

			// bImport=true: 재임포트로 mtl 값이 바뀌었으면 이미 로드된 머티리얼도 강제로 다시 만든다.
			// 주의: 이 머티리얼을 이미 참조 중인 다른 메시/컴포넌트가 있으면 그쪽은 옛 포인터가 댕글링된다
			// (RebindReferences가 지금은 UStaticMesh만 처리함 — 리플렉션으로 일반화하기 전까지의 임시 상태).
			UAsset* MaterialAsset = AssetManager->LoadAsset(MaterialAssetName, true);
			UMaterial* Material = MaterialAsset ? MaterialAsset->Cast<UMaterial>() : nullptr;

			UE_LOG("[StaticMeshLoader] material slot: name=%s key=%s found_in_mtl=%d material=%p",
				MaterialName.CStr(), MaterialAssetKey.CStr(), FoundMaterial != nullptr, (void*)Material);

			NewMesh->AddMaterial(Material);
			MaterialKeys.Add(MaterialAssetName);
		}

		NewMesh->SetMaterialKeys(MaterialKeys);

		UE_LOG("[StaticMeshLoader] baked: key=%s materials=%u sections=%u",
			BakedKey.ToString().CStr(), NewMesh->GetMaterialCount(), NewMesh->GetSectionCount());

		std::filesystem::create_directories(BakedPath.parent_path());
		FArchiveFileWriter Writer(BakedPath);
		NewMesh->Serialize(Writer);

		// 굽기 전용으로 임시로 만든 인스턴스라, GPU 리소스도 없이 바로 버린다.
		NewMesh->Destroy();
	}

	FFileAssetSource* BakedSource = new FFileAssetSource(InFileManager, BakedPath);
	AssetManager->RegisterAsset<FStaticMeshAssetLoader>(BakedKey, BakedSource, Renderer, *AssetManager);

	if (!bIsReimport)
	{
		AssetManager->SetImportSource(BakedKey, new FFileAssetSource(InFileManager, SourceObjPath));
	}

	return BakedKey;
}

UAsset* FStaticMeshAssetLoader::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
	FFileAssetSource& FileSource = static_cast<FFileAssetSource&>(AssetSource);

	UStaticMesh* NewMesh = FObjectFactory::ConstructObject<UStaticMesh>(AssetName);

	FArchiveFileReader Reader(FileSource.GetFilePath());
	NewMesh->Serialize(Reader);
	NewMesh->MarkDirty(false);

	// .uasset에는 UMaterial* 대신 키만 저장돼 있다. 여기서 실제 인스턴스로 다시 묶는다.
	// 이 세션에서 한 번도 Import()를 거치지 않은 키라면 아직 FAssetManager에 등록조차 안 돼 있어서
	// nullptr로 남는다 — 지금은 프로젝트 시작 시 에셋을 스캔해서 미리 등록해주는 게 없어서 생기는 한계다.
	for (const FName& Key : NewMesh->GetMaterialKeys())
	{
		UMaterial* Material = AssetManager->GetAssetAs<UMaterial>(Key, true);
		NewMesh->AddMaterial(Material);
	}

	NewMesh->BuildRenderBuffers(Renderer);

	UE_LOG("[StaticMeshLoader] loaded from uasset: name=%s materials=%u sections=%u",
		AssetName.ToString().CStr(), NewMesh->GetMaterialCount(), NewMesh->GetSectionCount());

	return NewMesh;
}

void FStaticMeshAssetLoader::UnloadAsset(UAsset* Asset)
{
	UStaticMesh* Mesh = Asset ? Asset->Cast<UStaticMesh>() : nullptr;
	if (!Mesh)
	{
		return;
	}

	// 머티리얼은 FAssetManager가 이름으로 관리하는 독립된 에셋이다(다른 메시와 공유될 수 있다).
	// 텍스처와 마찬가지로 이 메시가 소유권을 갖지 않으므로 슬롯만 비운다.
	Mesh->ClearMaterials();
}

UAsset* FMaterialAssetLoader::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
	FMaterialAssetSource& MaterialSource = static_cast<FMaterialAssetSource&>(AssetSource);

	UMaterial* Material = FObjectFactory::ConstructObject<UMaterial>(AssetName, Renderer);

	UE_LOG("[MaterialLoader] construct: name=%s has_data=%d instance=%p",
		AssetName.ToString().CStr(), MaterialSource.HasMaterialData(), (void*)Material);

	if (!MaterialSource.HasMaterialData())
	{
		// mtl에 이 이름이 없었다 — UMaterial의 기본값(불투명 흰색 등)을 그대로 쓴다.
		return Material;
	}

	const FMaterialData& Data = MaterialSource.GetMaterialData();

	Material->SetSpecularPower(Data.SpecularPower);
	Material->SetOpticalDensity(Data.OpticalDensity);
	Material->SetTransparency(Data.Transparency);
	Material->SetIlluminationModel(Data.IlluminationModel);
	Material->SetAmbientColor(Data.AmbientColor);
	Material->SetDiffuseColor({ Data.DiffuseColor, 1.0f });
	Material->SetSpecularColor(Data.SpecularColor);
	Material->SetEmissiveColor(Data.EmissiveColor);
	Material->SetTransmissionFilter(Data.TransmissionFilter);

	auto LoadTexture = [&](const FString& Filename) -> UTexture2D* {
		if (Filename.empty()) {
			return nullptr;
		}

		std::filesystem::path TexturePath = MaterialSource.GetObjDirectory() / Filename.CStr();

		// 프로젝트 루트 기준 상대경로가 키 — 다른 컴퓨터에서도 같은 키가 나오게.
		FString TexturePathString(MaterialSource.GetFileManager().MakeRelativeToRoot(TexturePath).string());
		FName TextureAssetName(TexturePathString);

		FFileAssetSource* TextureSource = new FFileAssetSource(MaterialSource.GetFileManager(), TexturePath);

		FAssetManager& AssetManager = FAssetManager::Get();
		AssetManager.RegisterAsset<FTexture2DAssetLoader>(TextureAssetName, TextureSource, Renderer);

		// bImport=true: 재임포트로 텍스처 파일이 바뀌었으면 이미 로드된 것도 강제로 다시 읽는다.
		// 주의: 이미 이 텍스처를 참조 중인 다른 머티리얼이 있으면 그쪽은 옛 포인터가 댕글링된다(위와 동일한 임시 상태).
		UAsset* TextureAsset = AssetManager.LoadAsset(TextureAssetName, true);
		UTexture2D* Texture = TextureAsset ? TextureAsset->Cast<UTexture2D>() : nullptr;

		UE_LOG("[MaterialLoader] texture: file=%s key=%s texture=%p", Filename.CStr(), TexturePathString.CStr(), (void*)Texture);

		return Texture;
		};

	Material->SetAmbientTexture(LoadTexture(Data.AmbientColorMapFilename));
	Material->SetDiffuseTexture(LoadTexture(Data.DiffuseColorMapFilename));
	Material->SetSpecularTexture(LoadTexture(Data.SpecularColorMapFilename));
	Material->SetBumpTexture(LoadTexture(Data.BumpMapFilename));

	return Material;
}

void FMaterialAssetLoader::UnloadAsset(UAsset* Asset)
{
	// 텍스처는 FAssetManager가 이름으로 따로 관리하므로 여기서 건드리지 않는다.
}
