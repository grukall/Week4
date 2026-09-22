#include "StaticMesh.h"
#include "Assets.h"
#include "Vector.h"
#include "TArray.h"
#include "FileManager.h"
#include "Material.h"
#include "FAssetManager.h"
#include "FLogManager.h"
#include "Archive.h"
#include "FMaterialAssetLoader.h"

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

	FFileAssetSource FileSource(InFileManager, SourceObjPath);
	FString FileContent = FileSource.ReadFileToString();
	FObjImporter Importer;
	FStaticMesh StaticMesh;
	TArray<FString> MaterialFiles;
	TMap<FString, FName> MaterialNameToAssetKey;

	StaticMesh.PathFileName = FString(FileSource.GetFilePath().string());
	Importer.LoadObjModel(FileContent, StaticMesh, MaterialFiles);

	for (FString filename : MaterialFiles)
	{
		std::filesystem::path ObjDirectory = FileSource.GetFilePath().parent_path();
		std::filesystem::path MtlPath = ObjDirectory / filename.CStr();

		FMaterialAssetLoader* MatLoader = AssetManager->GetOrCreateLoader<FMaterialAssetLoader>(Renderer, *AssetManager);
		TArray<FName> ImportedKeys = MatLoader->Import(MtlPath, InFileManager);

		// 머티리얼 키는 구워진 .uasset 경로이고, 파일 이름이 곧 mtl의 머티리얼 이름이다.
		// obj가 참조하는 이름(usemtl)으로 키를 찾을 수 있게 stem으로 매핑해둔다.
		for (const FName& Key : ImportedKeys)
		{
			FString KeyStr = Key.ToString();
			FString MatName(std::filesystem::path(KeyStr.CStr()).stem().string());
			MaterialNameToAssetKey.Add(MatName, Key);
		}
	}

	if (bNeedsBake)
	{
		TArray<FName> MaterialKeys;

		for (const FString& MaterialName : StaticMesh.Materials) {
			FName* Found = MaterialNameToAssetKey.Find(MaterialName);
			if (Found)
			{
				MaterialKeys.Add(*Found);
			}
			else
			{
				UE_LOG("[StaticMeshLoader] WARNING: material '%s' not found in any mtl", MaterialName.CStr());
				MaterialKeys.Add(FName(MaterialName));
			}
		}

		UStaticMesh* NewMesh = FObjectFactory::ConstructObject<UStaticMesh>(BakedKey);
		NewMesh->SetMaterialKeys(MaterialKeys);
		NewMesh->SetData(StaticMesh.Vertices, StaticMesh.Indices, StaticMesh.Sections);

		UE_LOG("[StaticMeshLoader] baked: key=%s materials=%u sections=%u",
			BakedKey.ToString().CStr(), NewMesh->GetMaterialCount(), NewMesh->GetSectionCount());

		std::filesystem::create_directories(BakedPath.parent_path());
		FArchiveFileWriter Writer(BakedPath);
		FString ClassName = NewMesh->GetRuntimeClass()->Name;
		Writer << ClassName;
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

FName FStaticMeshAssetLoader::ImportPrimitive(const FName& Name, const std::filesystem::path& SourceFilePath, FFileManager& InFileManager, const FVertexSimple* InVertices, uint32 InVertexCount, const uint32* InIndices, uint32 InIndexCount)
{
	{
		std::filesystem::path BakedPath = std::filesystem::path("Assets/Baked") / (std::string(Name.ToString().CStr()) + ".uasset");

		//.h 파일이 .uasset보다 최신이면(=코드에서 정점 배열을 고친 뒤 아직 안 구웠으면) 다시 굽는다.
		bool bNeedsBake = true;
		if (std::filesystem::exists(BakedPath) && std::filesystem::exists(SourceFilePath))
		{
			bNeedsBake = std::filesystem::last_write_time(SourceFilePath) > std::filesystem::last_write_time(BakedPath);
		}

		if (bNeedsBake)
		{
			TArray<FVertexSimple> Vertices;
			Vertices.Reserve(InVertexCount);
			for (uint32 i = 0; i < InVertexCount; ++i)
			{
				Vertices.Add(InVertices[i]);
			}

			TArray<uint32> Indices;
			Indices.Reserve(InIndexCount);
			for (uint32 i = 0; i < InIndexCount; ++i)
			{
				Indices.Add(InIndices[i]);
			}

			TArray<FStaticMeshSection> Sections;
			Sections.Add({ 0, InIndexCount, 0 });

			UStaticMesh* TempMesh = FObjectFactory::ConstructObject<UStaticMesh>(Name);
			TempMesh->SetData(Vertices, Indices, Sections);

			std::filesystem::create_directories(BakedPath.parent_path());
			FArchiveFileWriter Writer(BakedPath);
			FString ClassName = TempMesh->GetRuntimeClass()->Name;
			Writer << ClassName;
			TempMesh->Serialize(Writer);
			TempMesh->Destroy();

			UE_LOG("[StaticMeshLoader] primitive baked: name=%s path=%s", Name.ToString().CStr(), BakedPath.string().c_str());
		}

		// 굽기는 건너뛰어도 등록은 매번 해야 한다 — AssetMetaInfoMap은 프로세스마다 비어서 시작한다.
		FFileAssetSource* BakedSource = new FFileAssetSource(InFileManager, BakedPath);
		AssetManager->RegisterAsset<FStaticMeshAssetLoader>(Name, BakedSource, Renderer, *AssetManager);

		return Name;
	}
}

UAsset* FStaticMeshAssetLoader::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
	FFileAssetSource& FileSource = static_cast<FFileAssetSource&>(AssetSource);

	UStaticMesh* NewMesh = FObjectFactory::ConstructObject<UStaticMesh>(AssetName);

	FArchiveFileReader Reader(FileSource.GetFilePath());
	FString ClassName;
	Reader << ClassName;
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