#include "StaticMesh.h"
#include "Assets.h"
#include "Vector.h"
#include "TArray.h"
#include "FileManager.h"

UAsset* FStaticMeshAssetLoader::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
	FFileAssetSource& FileSource = static_cast<FFileAssetSource&>(AssetSource);
	FString FileContent = FileSource.ReadFileToString();
	FObjImporter Importer = FObjImporter{};
	FStaticMesh StaticMesh{};
	TArray<FString> MaterialFiles;

	StaticMesh.PathFileName = FString(FileSource.GetFilePath().string());
	Importer.LoadObjModel(FileContent, StaticMesh, MaterialFiles);

	TArray<FMaterialData> Materials;
	for (FString filename : MaterialFiles)
	{
		std::filesystem::path ObjDirectory = FileSource.GetFilePath().parent_path();
		std::filesystem::path MtlPath = ObjDirectory / filename.CStr();
		FString MaterialFileContent = FileSource.GetFileManager().ReadFileToString(MtlPath);
		Importer.ParseMtlFile(MaterialFileContent, Materials);
	}

	TArray<FVertexSimple> Vertices;
	ToFVertexSimple(StaticMesh.Vertices, Vertices);

	UStaticMesh* NewMesh = FObjectFactory::ConstructObject<UStaticMesh>(
		AssetName,
		Renderer,
		Vertices.Data(),
		Vertices.Num(),
		StaticMesh.Indices.Data(),
		StaticMesh.Indices.Num(),
		StaticMesh.Sections.Data(),
		StaticMesh.Sections.Num()
	);

	return NewMesh;
}

void FStaticMeshAssetLoader::UnloadAsset(UAsset* Asset)
{
	// TODO
}

void FStaticMeshAssetLoader::ToFVertexSimple(const TArray<FNormalVertex>& NormalVertices, TArray<FVertexSimple>& Vertices)
{
	for (auto& NormalVertice : NormalVertices)
	{
		Vertices.Add
		({
			NormalVertice.Pos.x, NormalVertice.Pos.y, NormalVertice.Pos.z,
			0.0f, 0.0f, 0.0f, 0.0f,
			NormalVertice.UV.X, NormalVertice.UV.Y,
			NormalVertice.Normal.x, NormalVertice.Normal.y, NormalVertice.Normal.z
		});
	}
}