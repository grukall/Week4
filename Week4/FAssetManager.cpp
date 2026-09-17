#include "FAssetManager.h"
#include "LaunchEngineLoop.h"

FAssetManager::~FAssetManager()
{
	for (auto& pair : LoadedAssets)
	{
		if (pair.second)
		{
			pair.second->Destroy();
		}
	}
	LoadedAssets.Empty();
	AssetMetaInfoMap.Empty();

	for (FAssetLoader* loader : OwnedLoaders)
	{
		delete loader;
	}
	OwnedLoaders.Empty();

	for (FAssetSource* source : OwnedSources)
	{
		delete source;
	}
	OwnedSources.Empty();
}

FAssetManager& FAssetManager::Get()
{
	return *GEngineLoop.GetAssetManager();
}

void FAssetManager::RegisterAsset(const FName& AssetName, FAssetLoader* AssetLoader, FAssetSource* AssetSource)
{
	if (AssetMetaInfoMap.Contains(AssetName) || LoadedAssets.Contains(AssetName))
	{
		return;
	}

	FAssetMetaInfo metaInfo;
	metaInfo.AssetName = AssetName;
	metaInfo.AssetLoader = AssetLoader;
	metaInfo.AssetSource = AssetSource;

	// 같은 로더가 여러 에셋에 쓰이므로 중복 없이 소유 목록에 담는다.
	if (AssetLoader)
	{
		bool bAlreadyOwned = false;
		for (FAssetLoader* loader : OwnedLoaders)
		{
			if (loader == AssetLoader)
			{
				bAlreadyOwned = true;
				break;
			}
		}

		if (!bAlreadyOwned)
		{
			OwnedLoaders.Add(AssetLoader);
		}
	}

	if (AssetSource)
	{
		bool bAlreadyOwned = false;
		for (FAssetSource* source : OwnedSources)
		{
			if (source == AssetSource)
			{
				bAlreadyOwned = true;
				break;
			}
		}

		if (!bAlreadyOwned)
		{
			OwnedSources.Add(AssetSource);
		}
	}

	AssetMetaInfoMap.Add(AssetName, metaInfo);
}

void FAssetManager::RegisterAsset(UAsset* Asset)
{
	if (!Asset)
	{
		return;
	}

	const FName& AssetName = Asset->GetAssetName();

	if (AssetMetaInfoMap.Contains(AssetName) || LoadedAssets.Contains(AssetName))
	{
		return;
	}

	FAssetMetaInfo metaInfo;
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
	UAsset* asset = GetAsset(AssetName);
	if (asset)
	{
		FAssetLoader* assetLoader = AssetMetaInfoMap.Contains(AssetName) ? AssetMetaInfoMap[AssetName].AssetLoader : nullptr;
		if (assetLoader)
		{
			assetLoader->UnloadAsset(asset);
		}
		LoadedAssets.Remove(AssetName);
		asset->Destroy();
	}
}

UAsset* FAssetManager::LoadAsset(const FName& AssetName)
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

	UAsset* asset = metaInfo.AssetLoader->LoadAsset(AssetName, *metaInfo.AssetSource);
	if (asset)
	{
		LoadedAssets.Add(AssetName, asset);
	}

	return asset;
}

UAsset* FAssetManager::GetAsset(const FName& AssetName, bool loadIfNotLoaded)
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
