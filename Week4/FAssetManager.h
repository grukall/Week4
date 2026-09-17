#pragma once

#include "Core.h"
#include "UAsset.h"
#include "TMap.h"
#include "TArray.h"

struct FAssetMetaInfo
{
	FName AssetName;
	FAssetLoader* AssetLoader = nullptr;
	FAssetSource* AssetSource = nullptr;
};

class FAssetManager
{
public:
	FAssetManager() = default;
	~FAssetManager();

	static FAssetManager& Get();

	// 로더와 소스의 소유권은 에셋 매니저가 가져간다. 같은 로더를 여러 에셋에 넘겨도 된다.
	void RegisterAsset(const FName& AssetName, FAssetLoader* AssetLoader, FAssetSource* AssetSource);
	void RegisterAsset(UAsset* Asset);
	void UnregisterAsset(const FName& AssetName);

	UAsset* LoadAsset(const FName& AssetName);
	UAsset* GetAsset(const FName& AssetName, bool loadIfNotLoaded = false);

	template <typename T>
	T* GetAssetAs(const FName& AssetName, bool loadIfNotLoaded = false)
	{
		UAsset* asset = GetAsset(AssetName, loadIfNotLoaded);
		if (asset)
		{
			return asset->Cast<T>();
		}

		return nullptr;
	}

	void UnloadAsset(const FName& AssetName);

	template <typename Func>
	void ForEachMetaInfo(Func&& func)
	{
		for (auto& pair : AssetMetaInfoMap)
		{
			func(pair.second);
		}
	}

private:
	TMap<FName, FAssetMetaInfo, FNameHasher> AssetMetaInfoMap;
	TMap<FName, UAsset*, FNameHasher> LoadedAssets;

	// 등록된 로더와 소스를 중복 없이 모아 두고 소멸 시점에 정리한다.
	TArray<FAssetLoader*> OwnedLoaders;
	TArray<FAssetSource*> OwnedSources;
};
