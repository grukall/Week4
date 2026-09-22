#include "Material.h"
#include "Json/json.hpp"
#include "Archive.h"

void UMaterial::Initialize(const FName& InAssetName, URenderer& InRenderer)
{
	UAsset::Initialize(InAssetName);
}

void UMaterial::SerializeClass(json::JSON& outJson) const
{
	//Super::SerializeClass(outJson);

	json::JSON& propertiesJson = outJson["Properties"];

	propertiesJson["DiffuseColor"] = json::JSON::Make(json::JSON::Class::Array);
	propertiesJson["DiffuseColor"].append(DiffuseColor.x);
	propertiesJson["DiffuseColor"].append(DiffuseColor.y);
	propertiesJson["DiffuseColor"].append(DiffuseColor.z);
	propertiesJson["DiffuseColor"].append(DiffuseColor.w);

	propertiesJson["DiffuseTexture"] = DiffuseTextureGuid.ToString();
}

void UMaterial::DeserializeClass(const json::JSON& inJson)
{
	//Super::DeserializeClass(inJson);

	const json::JSON& propertiesJson = inJson.at("Properties");

	if (!propertiesJson.hasKey("DiffuseColor")) {
		throw std::runtime_error("Invalid Json format for DiffuseColor");
	}
	if (!propertiesJson.hasKey("DiffuseTexture")) {
		throw std::runtime_error("Invalid Json format for DiffuseTexture");
	}

	const json::JSON& colorJson = propertiesJson.at("DiffuseColor");

	DiffuseColor.x = colorJson.at(0).ToFloat();
	DiffuseColor.y = colorJson.at(1).ToFloat();
	DiffuseColor.z = colorJson.at(2).ToFloat();
	DiffuseColor.w = colorJson.at(3).ToFloat();

	FGuid::Parse(FString(propertiesJson.at("DiffuseTexture").ToString()), DiffuseTextureGuid);
}
void UMaterial::CopyParametersFrom(const UMaterial& Other)
{
	SpecularPower = Other.SpecularPower;
	OpticalDensity = Other.OpticalDensity;
	Transparency = Other.Transparency;
	IlluminationModel = Other.IlluminationModel;

	AmbientColor = Other.AmbientColor;
	DiffuseColor = Other.DiffuseColor;
	SpecularColor = Other.SpecularColor;
	EmissiveColor = Other.EmissiveColor;

	AmbientTextureGuid = Other.AmbientTextureGuid;
	AmbientTexture = Other.AmbientTexture;
	DiffuseTextureGuid = Other.DiffuseTextureGuid;
	DiffuseTexture = Other.DiffuseTexture;
	SpecularTextureGuid = Other.SpecularTextureGuid;
	SpecularTexture = Other.SpecularTexture;
	BumpTextureGuid = Other.BumpTextureGuid;
	BumpTexture = Other.BumpTexture;

	UVScroll = Other.UVScroll;
	UVSpeed = Other.UVSpeed;
	TransmissionFilter = Other.TransmissionFilter;
}

UMaterial* UMaterial::DefaultMaterial = nullptr;
void UMaterial::InitDefaultMaterial(URenderer* Renderer)
{
	if (DefaultMaterial != nullptr || Renderer == nullptr) return;

	// "DefaultMaterial"�� FName("DefaultMaterial")���� ����� �����Ͽ� ����
	DefaultMaterial = FObjectFactory::ConstructObject<UMaterial>(
		FName("None"),
		*Renderer
	);

	// ����Ʈ �Ķ���� ����
	DefaultMaterial->SetAmbientColor({ 0.2f, 0.2f, 0.2f });
	DefaultMaterial->SetDiffuseColor({ 0.6f, 0.6f, 0.6f, 1.0f });
	DefaultMaterial->SetSpecularPower(32.0f);
}

void UMaterial::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << SpecularPower;
	Ar << OpticalDensity;
	Ar << Transparency;
	Ar << IlluminationModel;
	Ar << AmbientColor;
	Ar << DiffuseColor;
	Ar << SpecularColor;
	Ar << EmissiveColor;
	Ar << AmbientTextureGuid;
	Ar << DiffuseTextureGuid;
	Ar << SpecularTextureGuid;
	Ar << BumpTextureGuid;
	Ar << UVScroll;
	Ar << UVSpeed;
	Ar << TransmissionFilter;
}

void UMaterial::PostLoad(URenderer* Renderer)
{
	Super::PostLoad(Renderer);

	// TODO: Texture re-linking logic
}