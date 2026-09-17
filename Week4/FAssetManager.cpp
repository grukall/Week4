#include "FAssetManager.h"
#include "LaunchEngineLoop.h"

FAssetManager& FAssetManager::Get()
{
	return *GEngineLoop.GetAssetManager();
}

void FAssetManager::RegisterAsset(const FName& AssetName, const TSharedPtr<FAssetLoader>& AssetLoader, const TSharedPtr<FAssetSource>& AssetSource)
{
	if (AssetMetaInfoMap.Contains(AssetName) || LoadedAssets.Contains(AssetName))
	{
		return;
	}

	FAssetMetaInfo metaInfo;
	metaInfo.AssetType = AssetLoader->GetAssetType();
	metaInfo.AssetName = AssetName;
	metaInfo.AssetLoader = AssetLoader;
	metaInfo.AssetSource = AssetSource;

	AssetMetaInfoMap.Add(AssetName, metaInfo);
}

void FAssetManager::RegisterAsset(const TSharedPtr<FAsset>& Asset)
{
	const FName& AssetName = Asset->GetAssetName();

	if (AssetMetaInfoMap.Contains(AssetName) || LoadedAssets.Contains(AssetName))
	{
		return;
	}

	FAssetMetaInfo metaInfo;
	metaInfo.AssetType = Asset->GetAssetType();
	metaInfo.AssetName = AssetName;
	metaInfo.AssetLoader = nullptr;
	metaInfo.AssetSource = nullptr;

	AssetMetaInfoMap.Add(AssetName, metaInfo);
	LoadedAssets.Add(AssetName, Asset);
}

void FAssetManager::UnregisterAsset(const FName& AssetName)
{
	if (LoadedAssets.Contains(AssetName))
	{
		UnloadAsset(AssetName);
	}
	AssetMetaInfoMap.Remove(AssetName);
}

void FAssetManager::UnloadAsset(const FName& AssetName)
{
	TSharedPtr<FAsset> asset = GetAsset(AssetName);
	if (asset)
	{
		TSharedPtr<FAssetLoader> assetLoader = AssetMetaInfoMap[AssetName].AssetLoader;
		if (assetLoader)
		{
			assetLoader->UnloadAsset(asset);
		}
		LoadedAssets.Remove(AssetName);
	}
}

TSharedPtr<FAsset> FAssetManager::LoadAsset(const FName& AssetName)
{
	if (LoadedAssets.Contains(AssetName))
	{
		return LoadedAssets[AssetName];
	}

	if (!AssetMetaInfoMap.Contains(AssetName))
	{
		return nullptr;
	}

	const FAssetMetaInfo& metaInfo = AssetMetaInfoMap[AssetName];
	if (!metaInfo.AssetLoader || !metaInfo.AssetSource)
	{
		return nullptr;
	}

	TSharedPtr<FAsset> asset = metaInfo.AssetLoader->LoadAsset(AssetName, *metaInfo.AssetSource);
	if (asset)
	{
		LoadedAssets.Add(AssetName, asset);
	}

	return asset;
}

TSharedPtr<FAsset> FAssetManager::GetAsset(const FName& AssetName, bool loadIfNotLoaded)
{
	if (LoadedAssets.Contains(AssetName))
	{
		return LoadedAssets[AssetName];
	}

	if (loadIfNotLoaded)
	{
		return LoadAsset(AssetName);
	}

	return nullptr;
}
