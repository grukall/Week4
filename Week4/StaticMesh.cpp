#include "StaticMesh.h"
#include "Assets.h"
#include "Vector.h"
#include "TArray.h"
#include "FileManager.h"

UAsset* FStaticMeshAssetLoader::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
	UStaticMesh* NewMesh = FObjectFactory::ConstructObject<UStaticMesh>(AssetName);
	FFileAssetSource& FileSource = static_cast<FFileAssetSource&>(AssetSource);
	std::filesystem::path BinaryPath = "Assets/Cooked/" + std::string(AssetName.ToString().CStr()) + ".smesh";

	if (ShouldImport(AssetName, FileSource.GetFilePath(), BinaryPath))
	{
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

		NewMesh->SetData(
			StaticMesh.Vertices,
			StaticMesh.Indices,
			StaticMesh.Sections
		);

		NewMesh->MarkDirty(true);
	}
	else
	{
		FArchiveFileReader Reader(BinaryPath);
		NewMesh->Serialize(Reader);
		NewMesh->MarkDirty(false);
	}

	NewMesh->BuildRenderBuffers(Renderer);

	return NewMesh;
}

void FStaticMeshAssetLoader::UnloadAsset(UAsset* Asset)
{
	// TODO
}

bool FStaticMeshAssetLoader::ShouldImport(
	const FName AssetName,
	const std::filesystem::path& SourcePath,
	const std::filesystem::path& BinaryPath
)
{
	FString NameStr = AssetName.ToString();

	bool bRequiresImport = true;

	if (std::filesystem::exists(BinaryPath))
	{
		if (std::filesystem::exists(SourcePath))
		{
			auto SourceTime = std::filesystem::last_write_time(SourcePath);
			auto BinaryTime = std::filesystem::last_write_time(BinaryPath);

			if (BinaryTime >= SourceTime)
				bRequiresImport = false;
			else
				bRequiresImport = true;
		}
		else
		{
			bRequiresImport = false;
		}
	}

	return bRequiresImport;
}