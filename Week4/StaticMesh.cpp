#include "StaticMesh.h"
#include "Assets.h"
#include "Vector.h"
#include "TArray.h"
#include "FileManager.h"
#include "Material.h"
#include "FAssetManager.h"
UAsset* FStaticMeshAssetLoader::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
	FFileAssetSource& FileSource = static_cast<FFileAssetSource&>(AssetSource);
	FString FileContent = FileSource.ReadFileToString();
	FObjImporter Importer = FObjImporter{};
	FStaticMesh StaticMesh{};
	TArray<FString> MaterialFiles;

	StaticMesh.PathFileName = FString(FileSource.GetFilePath().string());
	Importer.LoadObjModel(FileContent, StaticMesh, MaterialFiles);

	TArray<FMaterialData> MaterialDatas;
	for (FString filename : MaterialFiles)
	{
		std::filesystem::path ObjDirectory = FileSource.GetFilePath().parent_path();
		std::filesystem::path MtlPath = ObjDirectory / filename.CStr();
		FString MaterialFileContent = FileSource.GetFileManager().ReadFileToString(MtlPath);
		Importer.ParseMtlFile(MaterialFileContent, MaterialDatas);
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

	for (const FString& MaterialName : StaticMesh.Materials) {
		const FMaterialData* FoundMaterial = nullptr;

		for (const FMaterialData& MaterialData : MaterialDatas) {
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