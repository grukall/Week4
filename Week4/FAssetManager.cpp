#include "FAssetManager.h"
#include "LaunchEngineLoop.h"
#include "Assets.h"
#include "UStaticMeshComponent.h"
#include "UObjectIterator.h"
#include "FLogManager.h"

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
		delete metaInfo.ImportSource;
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
	delete AssetMetaInfoMap[AssetName].ImportSource;
	AssetMetaInfoMap.Remove(AssetName);
}

bool FAssetManager::FindAssetByImportPath(const std::filesystem::path& ImportPath, FName& OutAssetName) const
{
	std::filesystem::path Normalized = std::filesystem::weakly_canonical(ImportPath);

	for (const auto& pair : AssetMetaInfoMap)
	{
		const FAssetMetaInfo& metaInfo = pair.second;
		if (!metaInfo.ImportSource)
		{
			continue;
		}

		if (std::filesystem::weakly_canonical(metaInfo.ImportSource->GetFilePath()) == Normalized)
		{
			OutAssetName = metaInfo.AssetName;
			return true;
		}
	}

	return false;
}

void FAssetManager::SetImportSource(const FName& AssetName, FFileAssetSource* ImportSource)
{
	if (!AssetMetaInfoMap.Contains(AssetName))
	{
		delete ImportSource;
		return;
	}

	FAssetMetaInfo& metaInfo = AssetMetaInfoMap[AssetName];
	if (metaInfo.ImportSource && metaInfo.ImportSource != ImportSource)
	{
		delete metaInfo.ImportSource;
	}

	metaInfo.ImportSource = ImportSource;
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
