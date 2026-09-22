#include "FMaterialAssetLoader.h"
#include "StaticMesh.h"
#include "FAssetManager.h"
#include "Assets.h"
#include <sstream>
#include <string_view>
#include <charconv>
#include "FLogManager.h"
#include "FileManager.h"
#include "Renderer.h"
#include "Material.h"
#include "Archive.h"

namespace
{
	// BakedDir/PreferredStem.uasset이 이미 있으면(다른 원본이 쓰고 있는 이름이면) 번호를 붙여
	// 비어있는 경로를 찾는다. 재임포트 여부는 호출자가 이미 판단했으므로 여기선 신규 임포트만 다룬다.
	std::filesystem::path MakeUniqueBakedPath(const std::filesystem::path& BakedDir, const FString& PreferredStem)
	{
		std::filesystem::path Candidate = BakedDir / (std::string(PreferredStem.CStr()) + ".uasset");
		if (!std::filesystem::exists(Candidate))
		{
			return Candidate;
		}

		for (int32 Suffix = 1; ; ++Suffix)
		{
			std::filesystem::path Numbered = BakedDir / (std::string(PreferredStem.CStr()) + "_" + std::to_string(Suffix) + ".uasset");
			if (!std::filesystem::exists(Numbered))
			{
				return Numbered;
			}
		}
	}
}

TArray<FName> FMaterialAssetLoader::Import(const std::filesystem::path& SourceMtlPath, FFileManager& InFileManager)
{
	TArray<FName> MaterialAssetNames;
	TArray<FMaterialData> MaterialDatas;
	FFileAssetSource FileSource(InFileManager, SourceMtlPath);
	FString FileContent = FileSource.ReadFileToString();

	if (!ParseMtlFile(FileContent, MaterialDatas))
	{
		return MaterialAssetNames;
	}

	for (FMaterialData& Data : MaterialDatas)
	{
		FString MaterialKey = FileSource.GetFileManager().MakeRelativeToRoot(SourceMtlPath).string();
		MaterialKey.Append("::");
		MaterialKey.Append(Data.Name);
		FName MaterialAssetName(MaterialKey);

		std::filesystem::path BakedDir = "Assets/Baked/Materials";
		std::filesystem::path CandidatePath = BakedDir / (std::string(Data.Name.CStr()) + ".uasset");
		bool bIsReimport = (AssetManager->GetAsset(MaterialAssetName) != nullptr);

		std::filesystem::path BakedPath;
		if (bIsReimport)
		{
			BakedPath = CandidatePath;
		}
		else
		{
			BakedPath = MakeUniqueBakedPath(BakedDir, Data.Name);
		}

		bool bNeedsBake = true;
		if (std::filesystem::exists(BakedPath) && std::filesystem::exists(SourceMtlPath))
		{
			bNeedsBake = std::filesystem::last_write_time(SourceMtlPath) > std::filesystem::last_write_time(BakedPath);
		}

		if (bNeedsBake)
		{
			FName BakedKey(FString(BakedPath.string()));
			UMaterial* TempMaterial = FObjectFactory::ConstructObject<UMaterial>(BakedKey, Renderer);

			TempMaterial->SetSpecularPower(Data.SpecularPower);
			TempMaterial->SetOpticalDensity(Data.OpticalDensity);
			TempMaterial->SetTransparency(Data.Transparency);
			TempMaterial->SetIlluminationModel(Data.IlluminationModel);
			TempMaterial->SetAmbientColor(Data.AmbientColor);
			TempMaterial->SetDiffuseColor({ Data.DiffuseColor, 1.0f });
			TempMaterial->SetSpecularColor(Data.SpecularColor);
			TempMaterial->SetEmissiveColor(Data.EmissiveColor);
			TempMaterial->SetTransmissionFilter(Data.TransmissionFilter);

			auto ProcessTexturePath = [&](const FString& Filename) -> FString {
				if (Filename.empty()) return Filename;
				std::filesystem::path AbsoluteTexPath = std::filesystem::weakly_canonical(SourceMtlPath.parent_path() / Filename.CStr());
				return FString(InFileManager.MakeRelativeToRoot(AbsoluteTexPath).string());
			};
			TempMaterial->SetAmbientTexturePath(ProcessTexturePath(Data.AmbientColorMapFilename));
			TempMaterial->SetDiffuseTexturePath(ProcessTexturePath(Data.DiffuseColorMapFilename));
			TempMaterial->SetSpecularTexturePath(ProcessTexturePath(Data.SpecularColorMapFilename));
			TempMaterial->SetBumpTexturePath(ProcessTexturePath(Data.BumpMapFilename));

			std::filesystem::create_directories(BakedPath.parent_path());
			FArchiveFileWriter Writer(BakedPath);
			FString ClassName = TempMaterial->GetRuntimeClass()->Name;
			Writer << ClassName;
			TempMaterial->Serialize(Writer);
			UE_LOG("[MaterialLoader] baked: key=%s name=%s",
				BakedKey.ToString().CStr(), Data.Name.CStr());

			TempMaterial->Destroy();
		}

		FFileAssetSource* BakedSource = new FFileAssetSource(InFileManager, BakedPath);
		AssetManager->RegisterAsset<FMaterialAssetLoader>(
			MaterialAssetName, BakedSource, Renderer, *AssetManager
		);

		if (!bIsReimport)
		{
			AssetManager->SetImportSource(MaterialAssetName, new FFileAssetSource(InFileManager, SourceMtlPath));
		}

		UE_LOG("[MaterialLoader] registered: logical_key=%s baked=%s",
			MaterialAssetName.ToString().CStr(), BakedPath.string().c_str());
		MaterialAssetNames.Add(MaterialAssetName);
	}
	
	return MaterialAssetNames;
}

bool FMaterialAssetLoader::ParseMtlFile(FString& FileContent, TArray<FMaterialData>& Materials)
{
	std::istringstream Stream(FileContent);
	std::string Line;

	if (Stream)
	{
		FString CurrentMaterialName = "";
		FMaterialData Data;
		while (std::getline(Stream, Line))
		{
			// Skip empty or comment line
			if (Line.empty() || Line[0] == '#')
				continue;

			std::istringstream iss(Line);
			std::string prefix;

			if (!(iss >> prefix))
				continue;

			if (prefix == "newmtl")
			{
				std::string MaterialName;
				if (iss >> MaterialName)
				{
					if (CurrentMaterialName != "")
					{
						Materials.Add(Data);
						Data = FMaterialData{};
					}
					CurrentMaterialName = FString(MaterialName);
					Data.Name = CurrentMaterialName;
				}
			}
			else if (prefix == "Ns")
			{
				float power;
				iss >> power;
				Data.SpecularPower = power;
			}
			else if (prefix == "Ni")
			{
				float density;
				iss >> density;
				Data.OpticalDensity = density;
			}
			else if (prefix == "d")
			{
				float transparency;
				iss >> transparency;
				Data.Transparency = transparency;
			}
			else if (prefix == "Tr")
			{
				float transparency;
				iss >> transparency;
				Data.Transparency = 1.0f - transparency;
			}
			else if (prefix == "Tf")
			{
				float x, y, z;
				iss >> x >> y >> z;
				Data.TransmissionFilter = { x, y, z };
			}
			else if (prefix == "illum")
			{
				int illum;
				iss >> illum;
				Data.IlluminationModel = illum;
			}
			else if (prefix == "Ka")
			{
				float x, y, z;
				iss >> x >> y >> z;
				Data.AmbientColor = { x, y, z };
			}
			else if (prefix == "Kd")
			{
				float x, y, z;
				iss >> x >> y >> z;
				Data.DiffuseColor = { x, y, z };
			}
			else if (prefix == "Ks")
			{
				float x, y, z;
				iss >> x >> y >> z;
				Data.SpecularColor = { x, y, z };
			}
			else if (prefix == "Ke")
			{
				float x, y, z;
				iss >> x >> y >> z;
				Data.EmissiveColor = { x, y, z };
			}
			else if (prefix == "map_Ka")
			{
				std::string filename;
				iss >> filename;
				Data.AmbientColorMapFilename = FString(filename);
			}
			else if (prefix == "map_Kd")
			{
				std::string filename;
				iss >> filename;
				Data.DiffuseColorMapFilename = FString(filename);
			}
			else if (prefix == "map_Ks")
			{
				std::string filename;
				iss >> filename;
				Data.SpecularColorMapFilename = FString(filename);
			}
			else if (prefix == "map_bump" || prefix == "bump")
			{
				std::string filename;
				iss >> filename;
				Data.BumpMapFilename = FString(filename);
			}
		}
		if (CurrentMaterialName != "")
		{
			Materials.Add(Data);
		}
	}
	else
	{
		UE_LOG_ERROR("Failed opening .mtl file");
		return false;
	}
	return true;
}

UAsset* FMaterialAssetLoader::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
	if (FFileAssetSource* FileSource = dynamic_cast<FFileAssetSource*>(&AssetSource))
	{
		UMaterial* Material = FObjectFactory::ConstructObject<UMaterial>(AssetName, Renderer);

		FArchiveFileReader Reader(FileSource->GetFilePath());
		FString ClassName;
		Reader << ClassName;
		Material->Serialize(Reader);
		Material->MarkDirty(false);

		UE_LOG("[MaterialLoader] loaded from uasset: name=%s path=%s",
			AssetName.ToString().CStr(), FileSource->GetFilePath().string().c_str());

		auto RebindTexture = [&](const FString& TexturePathString) -> UTexture2D*
		{
			if (TexturePathString.empty()) return nullptr;
			FName TextureAssetName(TexturePathString);

			if (!AssetManager->GetAsset(TextureAssetName, false))
			{
				std::filesystem::path AbsoluteTexPath = std::filesystem::absolute(TexturePathString.CStr());
				if (std::filesystem::exists(AbsoluteTexPath))
				{
					FFileAssetSource* TextureSource = new FFileAssetSource(FileSource->GetFileManager(), AbsoluteTexPath);
					AssetManager->RegisterAsset<FTexture2DAssetLoader>(TextureAssetName, TextureSource, Renderer);
				}
			}
			UAsset* TextureAsset = AssetManager->GetAsset(TextureAssetName, true);
			UTexture2D* Texture = TextureAsset ? TextureAsset->Cast<UTexture2D>() : nullptr;
			UE_LOG("[MaterialLoader] rebind texture: key=%s texture=%p", TexturePathString.CStr(), (void*)Texture);
			return Texture;
		};

		Material->SetAmbientTexture(RebindTexture(Material->GetAmbientTexturePath()));
		Material->SetDiffuseTexture(RebindTexture(Material->GetDiffuseTexturePath()));
		Material->SetSpecularTexture(RebindTexture(Material->GetSpecularTexturePath()));
		Material->SetBumpTexture(RebindTexture(Material->GetBumpTexturePath()));

		return Material;
	}

	FMaterialAssetSource& MaterialSource = static_cast<FMaterialAssetSource&>(AssetSource);

	UMaterial* Material = FObjectFactory::ConstructObject<UMaterial>(AssetName, Renderer);

	UE_LOG("[MaterialLoader] construct: name=%s has_data=%d instance=%p",
		AssetName.ToString().CStr(), MaterialSource.HasMaterialData(), (void*)Material);

	if (!MaterialSource.HasMaterialData())
	{
		return Material;
	}

	const FMaterialData& Data = MaterialSource.GetMaterialData();

	Material->SetSpecularPower(Data.SpecularPower);
	Material->SetOpticalDensity(Data.OpticalDensity);
	Material->SetTransparency(Data.Transparency);
	Material->SetIlluminationModel(Data.IlluminationModel);
	Material->SetAmbientColor(Data.AmbientColor);
	Material->SetDiffuseColor({ Data.DiffuseColor, 1.0f });
	Material->SetSpecularColor(Data.SpecularColor);
	Material->SetEmissiveColor(Data.EmissiveColor);
	Material->SetTransmissionFilter(Data.TransmissionFilter);

	auto LoadTexture = [&](const FString& Filename) -> UTexture2D* {
		if (Filename.empty()) {
			return nullptr;
		}

		std::filesystem::path TexturePath = MaterialSource.GetObjDirectory() / Filename.CStr();

		FString TexturePathString(MaterialSource.GetFileManager().MakeRelativeToRoot(TexturePath).string());
		FName TextureAssetName(TexturePathString);

		FFileAssetSource* TextureSource = new FFileAssetSource(MaterialSource.GetFileManager(), TexturePath);

		FAssetManager& AssetManager = FAssetManager::Get();
		AssetManager.RegisterAsset<FTexture2DAssetLoader>(TextureAssetName, TextureSource, Renderer);

		UAsset* TextureAsset = AssetManager.LoadAsset(TextureAssetName, true);
		UTexture2D* Texture = TextureAsset ? TextureAsset->Cast<UTexture2D>() : nullptr;

		UE_LOG("[MaterialLoader] texture: file=%s key=%s texture=%p", Filename.CStr(), TexturePathString.CStr(), (void*)Texture);

		return Texture;
		};

	Material->SetAmbientTexture(LoadTexture(Data.AmbientColorMapFilename));
	Material->SetDiffuseTexture(LoadTexture(Data.DiffuseColorMapFilename));
	Material->SetSpecularTexture(LoadTexture(Data.SpecularColorMapFilename));
	Material->SetBumpTexture(LoadTexture(Data.BumpMapFilename));

	return Material;
}

void FMaterialAssetLoader::UnloadAsset(UAsset* Asset)
{
	// 텍스처는 FAssetManager가 이름으로 따로 관리하므로 여기서 건드리지 않는다.
}
