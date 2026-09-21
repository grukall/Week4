#include "StaticMesh.h"
#include "Assets.h"
#include "Vector.h"
#include "TArray.h"
#include "FileManager.h"
#include "Material.h"
#include "FAssetManager.h"
#include "FLogManager.h"

UAsset* FStaticMeshAssetLoader::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
	UStaticMesh* NewMesh = FObjectFactory::ConstructObject<UStaticMesh>(AssetName);
	FFileAssetSource& FileSource = static_cast<FFileAssetSource&>(AssetSource);
	std::filesystem::path BinaryPath = "Assets/Baked/" + std::string(AssetName.ToString().CStr()) + ".uasset";

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

			if (!std::filesystem::exists(MtlPath))
			{
				UE_LOG_ERROR(std::format("{} doesn't exists!", MtlPath.string()).c_str());
				continue;
			}

			FString MaterialFileContent = FileSource.GetFileManager().ReadFileToString(MtlPath);
			Importer.ParseMtlFile(MaterialFileContent, Materials);
		}

		for (const FString& MaterialName : StaticMesh.Materials) {
			const FMaterialData* FoundMaterial = nullptr;

			for (const FMaterialData& MaterialData : Materials) {
				if (MaterialData.Name == MaterialName) {
					FoundMaterial = &MaterialData;
					break;
				}
			}

			UMaterial* Material = nullptr;

			if (FoundMaterial != nullptr) {
				Material = FObjectFactory::ConstructObject<UMaterial>(FName(MaterialName), Renderer);

				Material->SetSpecularPower(FoundMaterial->SpecularPower);

				Material->SetOpticalDensity(FoundMaterial->OpticalDensity);

				Material->SetTransparency(FoundMaterial->Transparency);

				Material->SetIlluminationModel(FoundMaterial->IlluminationModel);

				Material->SetAmbientColor(FoundMaterial->AmbientColor);

				Material->SetDiffuseColor({ FoundMaterial->DiffuseColor, 1.0f });

				Material->SetSpecularColor(FoundMaterial->SpecularColor);

				Material->SetEmissiveColor(FoundMaterial->EmissiveColor);

				Material->SetTransmissionFilter(FoundMaterial->TransmissionFilter);

				auto LoadTexture = [&](const FString& Filename) -> UTexture2D* {
					if (Filename.empty()) {
						return nullptr;
					}

					std::filesystem::path TexturePath = FileSource.GetFilePath().parent_path() / Filename.CStr();

					if (!std::filesystem::exists(TexturePath))
					{
						UE_LOG_ERROR(std::format("{} doesn't exists!", TexturePath.string()).c_str());
						return nullptr;
					}

					FString TexturePathString(TexturePath.string());
					FName TextureAssetName(TexturePathString);

					FFileAssetSource* TextureSource = new FFileAssetSource(FileSource.GetFileManager(), TexturePath);

					AssetManager->RegisterAsset(TextureAssetName, TextureLoader, TextureSource);

					return AssetManager->GetAssetAs<UTexture2D>(TextureAssetName, true);
					};

				Material->SetAmbientTexture(LoadTexture(FoundMaterial->AmbientColorMapFilename));

				Material->SetDiffuseTexture(LoadTexture(FoundMaterial->DiffuseColorMapFilename));

				Material->SetSpecularTexture(LoadTexture(FoundMaterial->SpecularColorMapFilename));

				Material->SetBumpTexture(LoadTexture(FoundMaterial->BumpMapFilename));
			}
			else {
				Material = FObjectFactory::ConstructObject<UMaterial>(FName(MaterialName), Renderer);
			}

			NewMesh->AddMaterial(Material);
		}

		NewMesh->SetData(
			StaticMesh.Vertices,
			StaticMesh.Indices,
			StaticMesh.Sections
		);

		std::filesystem::create_directories(BinaryPath.parent_path());
		FArchiveFileWriter Writer(BinaryPath);
		NewMesh->Serialize(Writer);
		NewMesh->MarkDirty(false);
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