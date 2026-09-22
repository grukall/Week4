#include "StaticMesh.h"
#include "Assets.h"
#include "Vector.h"
#include "TArray.h"
#include "FileManager.h"
#include "Material.h"
#include "FAssetManager.h"
#include "FAssetRegistry.h"
#include "FLogManager.h"
#include "Archive.h"
#include "FMaterialAssetLoader.h"

FName FStaticMeshAssetLoader::Import(const std::filesystem::path& SourceObjPath, FFileManager& InFileManager)
{
	FAssetRegistry& Registry = FAssetRegistry::Get();

	// 원본 obj의 GUID. 옆에 .obj.meta가 없으면 여기서 만들어진다.
	// 경로가 아니라 이 GUID가 "같은 원본인가"의 기준이라, obj를 옮기거나 이름을 바꿔도 따라간다.
	FGuid SourceGuid = Registry.GetOrCreateSourceGuid(SourceObjPath, InFileManager);

	// 이 원본에서 구워진 .uasset이 이미 있으면 그 자리에 다시 굽는다(= 재임포트).
	TArray<FGuid> ExistingAssets;
	std::filesystem::path BakedPath;
	bool bIsReimport = Registry.FindAssetsByImportSource(SourceGuid, ExistingAssets)
		&& Registry.FindPath(ExistingAssets[0], BakedPath);

	if (!bIsReimport)
	{
		FString PreferredStem(SourceObjPath.stem().string());
		BakedPath = FAssetManager::MakeUniqueBakedPath("Assets/Baked", PreferredStem);
	}

	FName BakedKey(FString(BakedPath.string()));

	// 이미 구워져 있고, 원본이 그 이후로 안 바뀌었으면 다시 파싱할 필요가 없다.
	bool bNeedsBake = true;
	if (std::filesystem::exists(BakedPath) && std::filesystem::exists(SourceObjPath))
	{
		bNeedsBake = std::filesystem::last_write_time(SourceObjPath) > std::filesystem::last_write_time(BakedPath);

		// 포맷 버전이 바뀌었으면 원본이 그대로여도 다시 구워야 한다(본문 레이아웃이 다르다).
		bNeedsBake |= !FAssetRegistry::IsBakedFileCurrent(BakedPath);
	}

	FFileAssetSource FileSource(InFileManager, SourceObjPath);
	FString FileContent = FileSource.ReadFileToString();
	FObjImporter Importer;
	FStaticMesh StaticMesh;
	TArray<FString> MaterialFiles;
	TMap<FString, FGuid> MaterialNameToGuid;

	StaticMesh.PathFileName = FString(FileSource.GetFilePath().string());
	Importer.LoadObjModel(FileContent, StaticMesh, MaterialFiles);

	for (FString filename : MaterialFiles)
	{
		std::filesystem::path ObjDirectory = FileSource.GetFilePath().parent_path();
		std::filesystem::path MtlPath = ObjDirectory / filename.CStr();

		FMaterialAssetLoader* MatLoader = AssetManager->GetOrCreateLoader<FMaterialAssetLoader>(Renderer, *AssetManager);
		// obj의 usemtl 이름으로 찾을 수 있게 이름 -> GUID로 모아둔다.
		for (const FImportedMaterial& Imported : MatLoader->Import(MtlPath, InFileManager))
		{
			MaterialNameToGuid.Add(Imported.Name, Imported.Guid);
		}
	}

	if (bNeedsBake)
	{
		TArray<FGuid> MaterialGuids;

		for (const FString& MaterialName : StaticMesh.Materials) {
			FGuid* Found = MaterialNameToGuid.Find(MaterialName);
			if (Found)
			{
				MaterialGuids.Add(*Found);
			}
			else
			{
				// 못 찾아도 자리는 채워야 한다 — 이 배열의 인덱스가 곧 섹션의 MaterialSlotIndex라,
				// 건너뛰면 뒤쪽 머티리얼이 전부 한 칸씩 밀린다. 빈 GUID는 로드할 때 걸러진다.
				UE_LOG_WARN("[StaticMeshLoader] material '%s' not found in any mtl", MaterialName.CStr());
				MaterialGuids.Add(FGuid());
			}
		}

		UStaticMesh* NewMesh = FObjectFactory::ConstructObject<UStaticMesh>(BakedKey);
		NewMesh->SetMaterialGuids(MaterialGuids);
		NewMesh->SetData(StaticMesh.Vertices, StaticMesh.Indices, StaticMesh.Sections);

		// 재임포트면 기존 .uasset의 GUID를 그대로 물려받는다.
		NewMesh->SetAssetGuid(Registry.AcquireGuidForBake(BakedPath));
		NewMesh->SetImportSourceGuid(SourceGuid);

		UE_LOG("[StaticMeshLoader] baked: key=%s materials=%u sections=%u",
			BakedKey.ToString().CStr(), NewMesh->GetMaterialCount(), NewMesh->GetSectionCount());

		std::filesystem::create_directories(BakedPath.parent_path());
		{
			FArchiveFileWriter Writer(BakedPath);
			NewMesh->Serialize(Writer);
		}

		Registry.Register(NewMesh->GetAssetGuid(), BakedPath, FString("UStaticMesh"), false, InFileManager);
		Registry.SetImportSource(NewMesh->GetAssetGuid(), SourceGuid);

		// 굽기 전용으로 임시로 만든 인스턴스라, GPU 리소스도 없이 바로 버린다.
		NewMesh->Destroy();
	}

	FFileAssetSource* BakedSource = new FFileAssetSource(InFileManager, BakedPath);
	AssetManager->RegisterAsset<FStaticMeshAssetLoader>(BakedKey, BakedSource, Renderer, *AssetManager);

	return BakedKey;
}
FName FStaticMeshAssetLoader::ImportPrimitive(const FName& Name, const std::filesystem::path& SourceFilePath, FFileManager& InFileManager, const FVertexSimple* InVertices, uint32 InVertexCount, const uint32* InIndices, uint32 InIndexCount)
{
	std::filesystem::path BakedPath = std::filesystem::path("Assets/Baked") / (std::string(Name.ToString().CStr()) + ".uasset");

	//.h 파일이 .uasset보다 최신이면(=코드에서 정점 배열을 고친 뒤 아직 안 구웠으면) 다시 굽는다.
	bool bNeedsBake = true;
	if (std::filesystem::exists(BakedPath) && std::filesystem::exists(SourceFilePath))
	{
		bNeedsBake = std::filesystem::last_write_time(SourceFilePath) > std::filesystem::last_write_time(BakedPath);

		// 포맷 버전이 바뀌었으면 .h가 그대로여도 다시 구워야 한다(본문 레이아웃이 다르다).
		bNeedsBake |= !FAssetRegistry::IsBakedFileCurrent(BakedPath);
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

		// 프리미티브도 일반 메시와 같은 규칙이다 — 정점을 고쳐 다시 구워도 GUID는 유지된다.
		TempMesh->SetAssetGuid(FAssetRegistry::Get().AcquireGuidForBake(BakedPath));

		// 프리미티브는 .h에서 굽지만 원본 GUID(.meta)를 두지 않는다 — 이름으로 직접 등록되므로
		// "같은 원본을 또 임포트했나"를 따질 일이 없고, 소스 트리에 .meta를 뿌릴 이유도 없다.

		std::filesystem::create_directories(BakedPath.parent_path());
		{
			FArchiveFileWriter Writer(BakedPath);
			TempMesh->Serialize(Writer);
		}

		FAssetRegistry::Get().Register(TempMesh->GetAssetGuid(), BakedPath, FString("UStaticMesh"), false, InFileManager);

		TempMesh->Destroy();

		UE_LOG("[StaticMeshLoader] primitive baked: name=%s path=%s", Name.ToString().CStr(), BakedPath.string().c_str());
	}

	// 굽기는 건너뛰어도 등록은 매번 해야 한다 — AssetMetaInfoMap은 프로세스마다 비어서 시작한다.
	FFileAssetSource* BakedSource = new FFileAssetSource(InFileManager, BakedPath);
	AssetManager->RegisterAsset<FStaticMeshAssetLoader>(Name, BakedSource, Renderer, *AssetManager);

	return Name;
}

UAsset* FStaticMeshAssetLoader::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
	FFileAssetSource& FileSource = static_cast<FFileAssetSource&>(AssetSource);

	UStaticMesh* NewMesh = FObjectFactory::ConstructObject<UStaticMesh>(AssetName);

	FArchiveFileReader Reader(FileSource.GetFilePath());
	NewMesh->Serialize(Reader);
	NewMesh->MarkDirty(false);

	// .uasset에는 UMaterial* 대신 GUID만 저장돼 있다. 여기서 실제 인스턴스로 다시 묶는다.
	// GUID -> 현재 경로는 레지스트리가 알고 있고, 그 경로가 곧 FAssetManager의 조회 키다.
	const TArray<FGuid>& MaterialGuids = NewMesh->GetMaterialGuids();
	for (uint32 SlotIndex = 0; SlotIndex < MaterialGuids.Num(); ++SlotIndex)
	{
		const FGuid& Guid = MaterialGuids[SlotIndex];

		std::filesystem::path MaterialPath;
		if (!Guid.IsValid() || !FAssetRegistry::Get().FindPath(Guid, MaterialPath))
		{
			// mtl에 없던 머티리얼이거나, 참조하던 .uasset이 사라진 경우.
			// AddMaterial과 달리 SetMaterial은 자리를 비워둬서 뒤 슬롯이 밀리지 않는다.
			UE_LOG_WARN("[StaticMeshLoader] material not found: slot=%u guid=%s",
				SlotIndex, Guid.ToString().CStr());
			NewMesh->SetMaterial(SlotIndex, nullptr);
			continue;
		}

		FName MaterialKey(FString(FileSource.GetFileManager().MakeRelativeToRoot(MaterialPath).string()));
		NewMesh->SetMaterial(SlotIndex, AssetManager->GetAssetAs<UMaterial>(MaterialKey, true));
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