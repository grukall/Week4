#pragma once

#include "Core.h"
#include "FName.h"

enum class EAssetType
{
	StaticMesh,
	Texture2D,
	Font,
	FontAtlas,
	SpriteAtlas
};

class FAsset
{
public:
	FAsset() = default;
	FAsset(const FName& InAssetName, EAssetType InAssetType)
		: AssetName(InAssetName)
		, AssetType(InAssetType)	
	{
	}

	virtual ~FAsset() = default;

	inline const FName& GetAssetName() const { return AssetName; }
	inline EAssetType GetAssetType() const { return AssetType; }

protected:
	FName AssetName;
	EAssetType AssetType;
};

class FAssetSource
{
public:
	virtual ~FAssetSource() = default; 
};

class FAssetLoader
{
public:
	virtual ~FAssetLoader() = default;

	virtual TSharedPtr<FAsset> LoadAsset(const FName& AssetName, FAssetSource& AssetSource) = 0;
	virtual void UnloadAsset(TSharedPtr<FAsset> Asset) = 0;
	virtual EAssetType GetAssetType() const = 0;
};
