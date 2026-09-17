#pragma once

#include "Core.h"
#include "FName.h"
#include "Object.h"

class UAsset : public UObject
{
	REFLECT_CLASS(UAsset, UObject)
public:
	UAsset() = default;
	virtual ~UAsset() = default;

	using UObject::Initialize;
	void Initialize(const FName& InAssetName)
	{
		UObject::Initialize();

		AssetName = InAssetName;
	}

	inline const FName& GetAssetName() const { return AssetName; }

protected:
	FName AssetName;
	FString AssetPath;
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
};
