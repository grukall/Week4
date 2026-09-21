#pragma once

#include "Core.h"
#include "UAsset.h"
#include "ObjImporter.h"

#include <filesystem>

class URenderer;
class FAssetManager;
class FTexture2DAssetLoader;
class FFileManager;
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
	FStaticMeshAssetLoader(URenderer& InRenderer, FAssetManager& InAssetManager)
		: Renderer(InRenderer), AssetManager(&InAssetManager) { }
	~FStaticMeshAssetLoader() = default;

	virtual UAsset* LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;
	virtual void UnloadAsset(UAsset* Asset) override;
private:
	URenderer& Renderer;
	FAssetManager* AssetManager;
	bool ShouldImport(const FName AssetName, const std::filesystem::path& SourcePath, const std::filesystem::path& BinaryPath);
};

// mtl 파일 하나에 머티리얼이 여러 개 들어있을 수 있어서(1파일=1에셋인 텍스처와 다름),
// 파일 경로가 아니라 이미 파싱된 FMaterialData 한 덩어리를 소스로 들고 있는다.
class FMaterialAssetSource : public FAssetSource
{
public:
	FMaterialAssetSource(FFileManager& InFileManager, const std::filesystem::path& InObjDirectory, const FMaterialData& InMaterialData, bool bInHasMaterialData)
		: FileManager(InFileManager), ObjDirectory(InObjDirectory), MaterialData(InMaterialData), bHasMaterialData(bInHasMaterialData) { }

	FFileManager& GetFileManager() const { return FileManager; }
	const std::filesystem::path& GetObjDirectory() const { return ObjDirectory; }
	const FMaterialData& GetMaterialData() const { return MaterialData; }

	// mtl에 이 이름의 머티리얼이 실제로 없었으면(오타 등) false.
	// 이 경우 UMaterial의 기본값을 그대로 쓴다 — Set*을 아무것도 안 부른다.
	bool HasMaterialData() const { return bHasMaterialData; }

private:
	FFileManager& FileManager;
	std::filesystem::path ObjDirectory;
	FMaterialData MaterialData;
	bool bHasMaterialData;
};

class FMaterialAssetLoader : public FAssetLoader
{
public:
	FMaterialAssetLoader(URenderer& InRenderer, FAssetManager& InAssetManager)
		: Renderer(InRenderer){ }
	~FMaterialAssetLoader() = default;

	virtual UAsset* LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;
	virtual void UnloadAsset(UAsset* Asset) override;

private:
	URenderer& Renderer;
};