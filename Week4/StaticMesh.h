#pragma once

#include "Core.h"
#include "UAsset.h"
#include "ObjImporter.h"

class URenderer;

struct FStaticMeshSection
{
	uint32 StartIndex;
	uint32 IndexCount;
	uint32 MaterialSlotIndex;
};

struct FStaticMesh
{
	FString PathFileName;
	TArray<FNormalVertex> Vertices;
	TArray<uint32> Indices;
	TArray<FStaticMeshSection> Sections;
	TArray<FString> Materials;
};

struct FMaterialData
{
	FString Name;
	float SpecularPower;
	float OpticalDensity;
	float Transparency;
	FVector3 TransmissionFilter;
	int IlluminationModel;
	FVector3 AmbientColor;
	FVector3 DiffuseColor;
	FVector3 SpecularColor;
	FVector3 EmissiveColor;
	FString AmbientColorMapFilename;
	FString DiffuseColorMapFilename;
	FString SpecularColorMapFilename;
	FString BumpMapFilename;
};

class FStaticMeshAssetLoader : public FAssetLoader
{
public:
	FStaticMeshAssetLoader(URenderer& InRenderer) : Renderer(InRenderer) {}
	~FStaticMeshAssetLoader() = default;

	virtual UAsset* LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;
	virtual void UnloadAsset(UAsset* Asset) override;
private:
	URenderer& Renderer;

	void ToFVertexSimple(const TArray<FNormalVertex>& NormalVertices, TArray<FVertexSimple>& Vertices);
};