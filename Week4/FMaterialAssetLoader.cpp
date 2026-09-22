#include "FMaterialAssetLoader.h"
#include "StaticMesh.h"
#include "FAssetManager.h"
#include "FAssetRegistry.h"
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

TArray<FImportedMaterial> FMaterialAssetLoader::Import(const std::filesystem::path& SourceMtlPath, FFileManager& InFileManager)
{
	TArray<FImportedMaterial> ImportedMaterials;
	if (!std::filesystem::exists(SourceMtlPath))
	{
		UE_LOG_WARN("Source MTL file does not exist: %s", SourceMtlPath.string().c_str());
		return ImportedMaterials;
	}

	TArray<FMaterialData> MaterialDatas;
	FFileAssetSource FileSource(InFileManager, SourceMtlPath);
	FString FileContent = FileSource.ReadFileToString();
	if (FileContent.Len() == 0)
	{
		UE_LOG_WARN("Source MTL file could not be read or is empty: %s", SourceMtlPath.string().c_str());
		return ImportedMaterials;
	}

	if (!ParseMtlFile(FileContent, MaterialDatas))
	{
		return ImportedMaterials;
	}

	// 이 mtl의 GUID. 여기서 나온 머티리얼들이 모두 같은 값을 원본으로 기록한다(1:N).
	FGuid SourceGuid = FAssetRegistry::Get().GetOrCreateSourceGuid(SourceMtlPath, InFileManager);

	for (FMaterialData& Data : MaterialDatas)
	{
		std::filesystem::path BakedDir = "Assets/Baked/Materials";
		std::filesystem::path CandidatePath = BakedDir / (std::string(Data.Name.CStr()) + ".uasset");

		// 이미 같은 이름으로 구워둔 게 있으면 그 자리에 다시 굽는다(= 재임포트).
		// 등록 여부가 아니라 파일 존재로 판단해야, 재시작 직후처럼 아무것도 로드되지 않은
		// 상태에서도 같은 판단이 나와서 _1, _2가 생기지 않는다.
		bool bIsReimport = std::filesystem::exists(CandidatePath);

		std::filesystem::path BakedPath;
		if (bIsReimport)
		{
			BakedPath = CandidatePath;
		}
		else
		{
			BakedPath = FAssetManager::MakeUniqueBakedPath(BakedDir, Data.Name);
		}

		// 조회 키는 구워진 .uasset의 루트 기준 상대경로다. ScanBakedAssets가 재시작 후 등록할 때
		// 쓰는 키와 정규화 방식이 같아야, GUID로 찾은 경로를 그대로 조회 키로 쓸 수 있다.
		FName MaterialAssetName(FString(InFileManager.MakeRelativeToRoot(BakedPath).string()));

		// 굽든 안 굽든 GUID는 알아야 한다(메시가 이 값을 참조로 저장한다).
		// 기존 파일이 있으면 그 GUID를 읽어오고, 없으면 새로 발급한다.
		FGuid MaterialGuid = FAssetRegistry::Get().AcquireGuidForBake(BakedPath);

		bool bNeedsBake = true;
		if (std::filesystem::exists(BakedPath) && std::filesystem::exists(SourceMtlPath))
		{
			bNeedsBake = std::filesystem::last_write_time(SourceMtlPath) > std::filesystem::last_write_time(BakedPath);

			// 포맷 버전이 바뀌었으면 원본이 그대로여도 다시 구워야 한다(본문 레이아웃이 다르다).
			bNeedsBake |= !FAssetRegistry::IsBakedFileCurrent(BakedPath);
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

			// mtl이 가리키는 이미지를 텍스처 .uasset으로 굽고, 그 GUID를 참조로 저장한다.
			auto ImportTexture = [&](const FString& Filename) -> FGuid {
				if (Filename.empty())
					return FGuid();

				const char* PathPtr = Filename.CStr();
				while (*PathPtr == '/' || *PathPtr == '\\')
					PathPtr++;

				std::filesystem::path AbsoluteTexPath = std::filesystem::weakly_canonical(SourceMtlPath.parent_path() / PathPtr);
				if (!std::filesystem::exists(AbsoluteTexPath))
				{
					UE_LOG_WARN("Material texture file does not exist, skipping: %s", AbsoluteTexPath.string().c_str());
					return FGuid();
				}

				FTexture2DAssetLoader* TextureLoader = AssetManager->GetOrCreateLoader<FTexture2DAssetLoader>(Renderer);
				return TextureLoader->Import(AbsoluteTexPath, InFileManager);
			};
			TempMaterial->SetAmbientTextureGuid(ImportTexture(Data.AmbientColorMapFilename));
			TempMaterial->SetDiffuseTextureGuid(ImportTexture(Data.DiffuseColorMapFilename));
			TempMaterial->SetSpecularTextureGuid(ImportTexture(Data.SpecularColorMapFilename));
			TempMaterial->SetBumpTextureGuid(ImportTexture(Data.BumpMapFilename));

			// 재임포트면 위에서 읽어둔 기존 GUID를 그대로 쓴다. 새로 발급하면 이 머티리얼을
			// 참조하던 메시들의 참조가 전부 끊어진다.
			TempMaterial->SetAssetGuid(MaterialGuid);

			TempMaterial->SetImportSourceGuid(SourceGuid);

			std::filesystem::create_directories(BakedPath.parent_path());
			{
				FArchiveFileWriter Writer(BakedPath);
				TempMaterial->Serialize(Writer);
			}

			FAssetRegistry::Get().Register(TempMaterial->GetAssetGuid(), BakedPath, FString("UMaterial"), false, InFileManager);
			FAssetRegistry::Get().SetImportSource(TempMaterial->GetAssetGuid(), SourceGuid);

			UE_LOG("[MaterialLoader] baked: key=%s name=%s guid=%s",
				BakedKey.ToString().CStr(), Data.Name.CStr(), TempMaterial->GetAssetGuid().ToString().CStr());

			TempMaterial->Destroy();
		}

		FFileAssetSource* BakedSource = new FFileAssetSource(InFileManager, BakedPath);
		AssetManager->RegisterAsset<FMaterialAssetLoader>(MaterialAssetName, BakedSource, Renderer, *AssetManager);

		UE_LOG("[MaterialLoader] registered: key=%s name=%s guid=%s",
			MaterialAssetName.ToString().CStr(), Data.Name.CStr(), MaterialGuid.ToString().CStr());

		ImportedMaterials.Add({ Data.Name, MaterialGuid, MaterialAssetName });
	}

	return ImportedMaterials;
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
		Material->Serialize(Reader);
		Material->MarkDirty(false);

		UE_LOG("[MaterialLoader] loaded from uasset: name=%s path=%s",
			AssetName.ToString().CStr(), FileSource->GetFilePath().string().c_str());

		// 저장된 건 GUID뿐이다. 레지스트리에 현재 위치를 물어 조회 키를 만든 뒤 로드한다.
		auto RebindTexture = [&](const FGuid& TextureGuid) -> UTexture2D*
		{
			std::filesystem::path TexturePath;
			if (!TextureGuid.IsValid() || !FAssetRegistry::Get().FindPath(TextureGuid, TexturePath))
			{
				return nullptr;
			}

			FName TextureAssetName(FString(FileSource->GetFileManager().MakeRelativeToRoot(TexturePath).string()));
			UAsset* TextureAsset = AssetManager->GetAsset(TextureAssetName, true);
			UTexture2D* Texture = TextureAsset ? TextureAsset->Cast<UTexture2D>() : nullptr;

			UE_LOG("[MaterialLoader] rebind texture: guid=%s texture=%p", TextureGuid.ToString().CStr(), (void*)Texture);
			return Texture;
		};

		Material->SetAmbientTexture(RebindTexture(Material->GetAmbientTextureGuid()));
		Material->SetDiffuseTexture(RebindTexture(Material->GetDiffuseTextureGuid()));
		Material->SetSpecularTexture(RebindTexture(Material->GetSpecularTextureGuid()));
		Material->SetBumpTexture(RebindTexture(Material->GetBumpTextureGuid()));

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
