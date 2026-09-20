#pragma once

#include "Core.h"
#include "UAsset.h"
#include "ObjImporter.h"

class URenderer;
class FAssetManager;
class FTexture2DAssetLoader;
struct FStaticMeshSection
{
	uint32 StartIndex;
	uint32 IndexCount;
	uint32 MaterialSlotIndex;
};

struct FStaticMesh
{
	FString PathFileName;
	TArray<FVertexSimple> Vertices;
	TArray<uint32> Indices;
	TArray<FStaticMeshSection> Sections;
	TArray<FString> Materials;
};

struct FMaterialData
{
	FString Name;
	float SpecularPower = 0.0f;
	float OpticalDensity = 1.0f;
	float Transparency = 1.0f;
	FVector3 TransmissionFilter;
	int IlluminationModel = 2;
	FVector3 AmbientColor = FVector3(1.0f, 1.0f, 1.0f);
	FVector3 DiffuseColor = FVector3(1.0f, 1.0f, 1.0f);
	FVector3 SpecularColor = FVector3(0.0f, 0.0f, 0.0f);
	FVector3 EmissiveColor = FVector3(0.0f, 0.0f, 0.0f);
	FString AmbientColorMapFilename;
	FString DiffuseColorMapFilename;
	FString SpecularColorMapFilename;
	FString BumpMapFilename;
};

class FStaticMeshAssetLoader : public FAssetLoader
{
public:
	FStaticMeshAssetLoader(URenderer& InRenderer, FAssetManager& InAssetManager, FTexture2DAssetLoader& InTextureLoader)
		: Renderer(InRenderer), AssetManager(&InAssetManager), TextureLoader(&InTextureLoader) { }
	~FStaticMeshAssetLoader() = default;

	virtual UAsset* LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;
	virtual void UnloadAsset(UAsset* Asset) override;
private:
	URenderer& Renderer;
	FAssetManager* AssetManager;
	FTexture2DAssetLoader* TextureLoader;

	bool ShouldImport(const FName AssetName, const std::filesystem::path& SourcePath, const std::filesystem::path& BinaryPath);
};