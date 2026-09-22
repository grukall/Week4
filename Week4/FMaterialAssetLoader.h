#pragma once

#include "Core.h"
#include "UAsset.h"
#include "ObjImporter.h"
#include <filesystem>

class URenderer;
class FAssetManager;
class FTexture2DAssetLoader;
class FFileManager;

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

// mtl 파일 하나에 머티리얼이 여러 개 들어있을 수 있어서(1파일=1에셋인 텍스처와 다름),
// 파일 경로가 아니라 이미 파싱된 FMaterialData 한 덩어리를 소스로 들고 있는다.
class FMaterialAssetSource : public FAssetSource
{
public:
	FMaterialAssetSource(FFileManager& InFileManager, const std::filesystem::path& InObjDirectory, const FMaterialData& InMaterialData, bool bInHasMaterialData)
		: FileManager(InFileManager), ObjDirectory(InObjDirectory), MaterialData(InMaterialData), bHasMaterialData(bInHasMaterialData) {
	}

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

// Import()가 돌려주는 한 건. 메시는 obj의 usemtl 이름으로 자기 슬롯에 맞는 GUID를 찾아야 하므로
// 이름과 GUID를 짝지어 돌려준다. AssetKey는 이번 세션의 FAssetManager 조회 키다(디스크에는 안 남는다).
struct FImportedMaterial
{
	FString Name;
	FGuid Guid;
	FName AssetKey;
};

class FMaterialAssetLoader : public FAssetLoader
{
public:
	FMaterialAssetLoader(URenderer& InRenderer, FAssetManager& InAssetManager)
		: Renderer(InRenderer), AssetManager(&InAssetManager) {
	}
	~FMaterialAssetLoader() = default;

	virtual UAsset* LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;
	virtual void UnloadAsset(UAsset* Asset) override;
	TArray<FImportedMaterial> Import(const std::filesystem::path& SourceObjPath, FFileManager& InFileManager);

private:
	URenderer& Renderer;
	FAssetManager* AssetManager;

	bool ParseMtlFile(FString& FileContent, TArray<FMaterialData>& Materials);
};