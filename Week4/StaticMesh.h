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

class FStaticMeshAssetLoader : public FAssetLoader
{
public:
	FStaticMeshAssetLoader(URenderer& InRenderer, FAssetManager& InAssetManager)
		: Renderer(InRenderer), AssetManager(&InAssetManager) { }
	~FStaticMeshAssetLoader() = default;

	// FAssetLoader 인터페이스: .uasset을 읽어 UObject로 복원만 한다. 텍스트 파싱은 하지 않는다.
	virtual UAsset* LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;
	virtual void UnloadAsset(UAsset* Asset) override;

	// obj/mtl을 파싱해서 .uasset으로 굽고 FAssetManager에 등록한다. 이미 같은 원본을 임포트한 적
	// 있으면(ImportSource로 판단) 그 자리에 재임포트, 처음 보는 원본이면 새 .uasset을 만든다.
	// 반환값은 그 결과로 정해진 .uasset 키 — 이걸로 AssetManager->LoadAsset(key, true)를 부르면 된다.
	FName Import(const std::filesystem::path& SourceObjPath, FFileManager& InFileManager);

private:
	URenderer& Renderer;
	FAssetManager* AssetManager;
};