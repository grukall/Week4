#pragma once

#include "Core.h"
#include "FAsset.h"
#include "TMap.h"
#include "TArray.h"

struct FAssetMetaInfo
{
	EAssetType AssetType;
	FName AssetName;
	TSharedPtr<FAssetLoader> AssetLoader;
	TSharedPtr<FAssetSource> AssetSource;
};

class FAssetManager
{
public:
	static FAssetManager& Get();
	void RegisterAsset(const FName& AssetName, const TSharedPtr<FAssetLoader>& AssetLoader, const TSharedPtr<FAssetSource>& AssetSource);
	void RegisterAsset(const TSharedPtr<FAsset>& Asset);
	void UnregisterAsset(const FName& AssetName);

	TSharedPtr<FAsset> LoadAsset(const FName& AssetName);
	TSharedPtr<FAsset> GetAsset(const FName& AssetName, bool loadIfNotLoaded = false);

	template <typename T>
	TSharedPtr<T> GetAssetAs(const FName& AssetName, bool loadIfNotLoaded = false)
	{
		TSharedPtr<FAsset> asset = GetAsset(AssetName, loadIfNotLoaded);
		if (asset)
		{
			return std::static_pointer_cast<T>(asset);
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
	TMap<FName, TSharedPtr<FAsset>, FNameHasher> LoadedAssets;
};