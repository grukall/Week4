#include "FAssetManager.h"
#include "FAssetRegistry.h"
#include "LaunchEngineLoop.h"
#include "Assets.h"
#include "UStaticMeshComponent.h"
#include "UObjectIterator.h"
#include "FLogManager.h"
#include "FileManager.h"
#include "FMaterialAssetLoader.h"


#include <filesystem>

namespace
{
	// AssetName이 경로 형태든("C:/.../chair.obj") 그냥 이름("CubeMesh")이든
	// std::filesystem::path::stem()이 둘 다 안전하게 처리해준다.
	FName ExtractStem(const FName& AssetName)
	{
		FString NameStr = AssetName.ToString();

		// Material 키는 "mtl경로::머티리얼이름" 형태라 실제 파일 경로가 아니다.
		// path::stem()에 그대로 넣으면 Windows가 "::" 뒤를 잘라버려서(NTFS 스트림 구분자와 겹침)
		// mtl 파일명만 남고 진짜 머티리얼 이름이 사라진다. "::"가 있으면 그 뒤쪽을 그대로 쓴다.
		int32 SeparatorIndex = NameStr.Find(FString("::"));
		if (SeparatorIndex != -1)
		{
			return FName(NameStr.RightChop(SeparatorIndex + 2));
		}

		std::filesystem::path Path(NameStr.CStr());
		return FName(Path.stem().string().c_str());
	}
}

std::filesystem::path FAssetManager::MakeUniqueBakedPath(const std::filesystem::path& BakedDir, const FString& PreferredStem)
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

FAssetManager::~FAssetManager()
{
	UE_LOG("[AssetManager] Shutdown: registered=%u", AssetMetaInfoMap.Num());

	for (auto& pair : AssetMetaInfoMap)
	{
		FAssetMetaInfo& metaInfo = pair.second;

		if (metaInfo.LoadedAsset)
		{
			// 로더가 붙어 있으면 거쳐야 부속 객체까지 정리된다.
			if (metaInfo.AssetLoader)
			{
				metaInfo.AssetLoader->UnloadAsset(metaInfo.LoadedAsset);
			}

			metaInfo.LoadedAsset->Destroy();
		}

		delete metaInfo.AssetSource;
	}
	AssetMetaInfoMap.Empty();

	for (FAssetLoader* loader : OwnedLoaders)
	{
		delete loader;
	}
	OwnedLoaders.Empty();
}

FAssetManager& FAssetManager::Get()
{
	return *GEngineLoop.GetAssetManager();
}

void FAssetManager::RegisterAssetInternal(const FName& AssetName, FAssetLoader* AssetLoader, FAssetSource* AssetSource)
{
	FName Stem = ExtractStem(AssetName);

	// 이미 등록된 이름이면 재임포트다. 방금 고른 새 소스로 교체하고, 옛 소스는 여기서 바로 지운다.
	if (AssetMetaInfoMap.Contains(AssetName))
	{
		FAssetMetaInfo& existing = AssetMetaInfoMap[AssetName];
		if (existing.AssetSource && existing.AssetSource != AssetSource)
		{
			delete existing.AssetSource;
		}

		existing.AssetLoader = AssetLoader;
		existing.AssetSource = AssetSource;
		existing.Stem = Stem;

		UE_LOG("[AssetManager] Register(replace source): key=%s stem=%s", AssetName.ToString().CStr(), Stem.ToString().CStr());
		return;
	}

	FAssetMetaInfo metaInfo;
	metaInfo.AssetName = AssetName;
	metaInfo.Stem = Stem;
	metaInfo.AssetLoader = AssetLoader;
	metaInfo.AssetSource = AssetSource;

	AssetMetaInfoMap.Add(AssetName, metaInfo);

	UE_LOG("[AssetManager] Register(new): key=%s stem=%s", AssetName.ToString().CStr(), Stem.ToString().CStr());
}

void FAssetManager::RegisterAsset(UAsset* Asset)
{
	if (!Asset)
	{
		return;
	}

	// 이 경로로 들어오는 에셋(엔진 내장 프로시저럴 메시 등)은 파일 경로가 없어서
	// 키와 표시 이름이 애초에 같다 — 둘 다 Asset이 이미 갖고 있는 이름을 그대로 쓴다.
	const FName& AssetName = Asset->GetAssetName();
	if (AssetMetaInfoMap.Contains(AssetName))
	{
		return;
	}

	FAssetMetaInfo metaInfo;
	metaInfo.AssetName = AssetName;
	metaInfo.Stem = AssetName;
	metaInfo.LoadedAsset = Asset;
	metaInfo.AssetClass = Asset->GetRuntimeClass();

	AssetMetaInfoMap.Add(AssetName, metaInfo);
}

void FAssetManager::UnregisterAsset(const FName& AssetName)
{
	if (!AssetMetaInfoMap.Contains(AssetName))
	{
		return;
	}

	UnloadAsset(AssetName);

	delete AssetMetaInfoMap[AssetName].AssetSource;
	AssetMetaInfoMap.Remove(AssetName);
}

void FAssetManager::ScanBakedAssets(const std::filesystem::path& BakedDir, URenderer& Renderer, FFileManager& FileManager)
{
	// 디스크를 훑는 것도, 헤더 포맷을 아는 것도 레지스트리 몫이다.
	// 여기서는 그 결과를 받아 "어떤 로더로 열 것인가"만 정한다.
	FAssetRegistry& Registry = FAssetRegistry::Get();
	Registry.ScanDirectory(BakedDir, FileManager);

	// 이미 다른 키(예: 프리미티브는 "CubeMesh" 같은 짧은 이름)로 등록된 .uasset은 다시 등록하지 않는다.
	TArray<std::filesystem::path> AlreadyKnownPaths;
	for (const auto& pair : AssetMetaInfoMap)
	{
		if (FFileAssetSource* Source = dynamic_cast<FFileAssetSource*>(pair.second.AssetSource))
		{
			AlreadyKnownPaths.Add(std::filesystem::weakly_canonical(Source->GetFilePath()));
		}
	}

	Registry.ForEachEntry([&](const FAssetRegistryEntry& Entry)
	{
		// 레지스트리는 BakedDir 밖의 원본 파일(.meta로 관리되는 png/obj 등)도 들고 있다.
		if (Entry.bIsSourceFile || !IsUnder(Entry.Path, BakedDir))
		{
			return;
		}

		std::filesystem::path Normalized = std::filesystem::weakly_canonical(Entry.Path);
		for (const std::filesystem::path& Known : AlreadyKnownPaths)
		{
			if (Known == Normalized)
			{
				return;
			}
		}

		FName Key(FString(FileManager.MakeRelativeToRoot(Entry.Path).string()));
		if (AssetMetaInfoMap.Contains(Key))
		{
			return;
		}

		// 클래스 이름 -> 로더 매핑. 새 굽는 에셋 타입이 생기면 여기만 늘리면 된다.
		const FClassInfo* AssetClass = nullptr;
		FAssetLoader* Loader = nullptr;

		if (Entry.ClassName.Equals(FString("UStaticMesh")))
		{
			AssetClass = UStaticMesh::GetClass();
			Loader = GetOrCreateLoader<FStaticMeshAssetLoader>(Renderer, *this);
		}
		else if (Entry.ClassName.Equals(FString("UMaterial")))
		{
			AssetClass = UMaterial::GetClass();
			Loader = GetOrCreateLoader<FMaterialAssetLoader>(Renderer, *this);
		}
		else if (Entry.ClassName.Equals(FString("UTexture2D")))
		{
			AssetClass = UTexture2D::GetClass();
			Loader = GetOrCreateLoader<FTexture2DAssetLoader>(Renderer);
		}

		if (!Loader)
		{
			UE_LOG_WARN("[AssetManager] ScanBakedAssets: unknown class '%s', skip: %s", Entry.ClassName.CStr(), Entry.Path.string().c_str());
			return;
		}

		RegisterAssetInternal(Key, Loader, new FFileAssetSource(FileManager, Entry.Path));
		AssetMetaInfoMap[Key].AssetClass = AssetClass;
		AssetMetaInfoMap[Key].Guid = Entry.Guid;

		// (원본과의 연결은 레지스트리가 스캔할 때 이미 역인덱스로 만들어뒀다.
		//  재임포트 판정은 여기가 아니라 FAssetRegistry::FindAssetsByImportSource가 한다.)

		UE_LOG("[AssetManager] ScanBakedAssets: registered (not loaded) key=%s class=%s guid=%s",
			Key.ToString().CStr(), Entry.ClassName.CStr(), Entry.Guid.ToString().CStr());
	});
}

void FAssetManager::UnloadAsset(const FName& AssetName)
{
	if (!AssetMetaInfoMap.Contains(AssetName))
	{
		return;
	}

	FAssetMetaInfo& metaInfo = AssetMetaInfoMap[AssetName];
	if (!metaInfo.LoadedAsset)
	{
		return;
	}

	UE_LOG("[AssetManager] Unload: key=%s stem=%s asset=%p", AssetName.ToString().CStr(), metaInfo.Stem.ToString().CStr(), (void*)metaInfo.LoadedAsset);

	if (metaInfo.AssetLoader)
	{
		metaInfo.AssetLoader->UnloadAsset(metaInfo.LoadedAsset);
	}

	metaInfo.LoadedAsset->Destroy();
	metaInfo.LoadedAsset = nullptr;
}

UAsset* FAssetManager::LoadAsset(const FName& AssetName, bool bImport)
{
	if (!AssetMetaInfoMap.Contains(AssetName))
	{
		return nullptr;
	}

	FAssetMetaInfo& metaInfo = AssetMetaInfoMap[AssetName];
	UAsset* previousAsset = nullptr;
	if (metaInfo.LoadedAsset)
	{
		if (!bImport)
		{
			UE_LOG("[AssetManager] Load(cache hit): key=%s asset=%p", AssetName.ToString().CStr(), (void*)metaInfo.LoadedAsset);
			return metaInfo.LoadedAsset;
		}

		// import인 경우는 기존 로드된 에셋을 덮어씌운다.
		// 로더를 거쳐야 메시가 들고 있던 머티리얼 같은 부속 객체까지 정리된다.
		// TODO : 정리가 아닌 정보 갱신으로 교체
		UE_LOG("[AssetManager] Load(force reimport): key=%s old_asset=%p", AssetName.ToString().CStr(), (void*)metaInfo.LoadedAsset);
		previousAsset = metaInfo.LoadedAsset;
		UnloadAsset(AssetName);
	}

	if (!metaInfo.AssetLoader || !metaInfo.AssetSource)
	{
		return nullptr;
	}

	// 로더에게는 조회 키가 아니라 표시 이름(stem)을 준다. 로더는 이걸 그대로 UAsset의 FName으로 쓴다.
	UAsset* asset = metaInfo.AssetLoader->LoadAsset(metaInfo.Stem, *metaInfo.AssetSource);
	metaInfo.LoadedAsset = asset;

	// 실제로 로드됐으니 진짜 클래스를 안다 — ScanBakedAssets가 미리 채워둔 값보다 이게 항상 정확하다.
	if (asset)
	{
		metaInfo.AssetClass = asset->GetRuntimeClass();
	}

	UE_LOG("[AssetManager] Load(fresh): key=%s stem=%s new_asset=%p", AssetName.ToString().CStr(), metaInfo.Stem.ToString().CStr(), (void*)asset);

	if (previousAsset && asset)
	{
		RebindReferences(previousAsset, asset);
	}

	return asset;
}

void FAssetManager::RebindReferences(UAsset* OldAsset, UAsset* NewAsset)
{
	// TODO : Reflection으로 일반화 작업
	if (UStaticMesh* NewMesh = NewAsset->Cast<UStaticMesh>())
	{
		int32 RebindCount = 0;
		for (TObjectIterator<UStaticMeshComponent> It(true); It; ++It)
		{
			// OldAsset은 이미 해제된 주소다. 값 비교에만 쓴다.
			if (It->GetStaticMesh() != OldAsset)
			{
				continue;
			}

			// 새 메시가 같은 주소에 잡혔을 수 있어 SetStaticMesh의 동일 포인터 조기 반환을 피한다.
			It->SetStaticMesh(nullptr);
			It->SetStaticMesh(NewMesh);
			++RebindCount;
		}

		UE_LOG("[AssetManager] Rebind: old=%p new=%p components=%d", (void*)OldAsset, (void*)NewAsset, RebindCount);
	}
}

UAsset* FAssetManager::GetAsset(const FName& AssetName, bool loadIfNotLoaded)
{
	if (AssetMetaInfoMap.Contains(AssetName) && AssetMetaInfoMap[AssetName].LoadedAsset)
	{
		return AssetMetaInfoMap[AssetName].LoadedAsset;
	}

	if (loadIfNotLoaded)
	{
		return LoadAsset(AssetName);
	}

	return nullptr;
}
