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

	// obj 없이 코드에 박혀있는 정점 배열(엔진 내장 프리미티브: Cube/Sphere/Plane 등)을 구울 때 쓴다.
	// SourceFilePath는 그 정점 배열이 선언된 .h 파일(예: "Week4/Cube.h") — 진짜 obj는 아니지만
	// 엄연히 실존하는 파일이라, Import()와 똑같이 mtime을 비교해서 낡았을 때만 다시 굽는다.
	// 키는 Name 그대로 유지한다(경로가 아님) — CubeComponent 등 기존 코드가 "CubeMesh" 같은
	// 짧은 이름으로 조회하므로, 키를 .uasset 경로로 바꾸면 그 조회들이 전부 깨진다.
	FName ImportPrimitive(const FName& Name, const std::filesystem::path& SourceFilePath, FFileManager& InFileManager, const FVertexSimple* InVertices, uint32 InVertexCount, const uint32* InIndices, uint32 InIndexCount);

private:
	URenderer& Renderer;
	FAssetManager* AssetManager;
};