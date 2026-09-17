#pragma once

#include "Core.h"
#include "FName.h"
#include "Object.h"

enum class EAssetType
{
	StaticMesh,
	Texture2D,
	Font,
	FontAtlas,
	SpriteAtlas
};

class UAsset : public UObject
{
	REFLECT_CLASS(UAsset, UObject)
public:
	UAsset() = default;
	virtual ~UAsset() = default;

	using UObject::Initialize;
	void Initialize(const FName& InAssetName, EAssetType InAssetType)
	{
		UObject::Initialize();

		AssetName = InAssetName;
		AssetType = InAssetType;
	}

	inline const FName& GetAssetName() const { return AssetName; }
	inline EAssetType GetAssetType() const { return AssetType; }

protected:
	FName AssetName;
	EAssetType AssetType = EAssetType::StaticMesh;
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

	virtual UAsset* LoadAsset(const FName& AssetName, FAssetSource& AssetSource) = 0;
	virtual void UnloadAsset(UAsset* Asset) = 0;
	virtual EAssetType GetAssetType() const = 0;
};
