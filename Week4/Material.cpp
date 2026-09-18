#include "Material.h"
#include "Json/json.hpp"

void UMaterial::SerializeClass(json::JSON& outJson) const
{
	//Super::SerializeClass(outJson);

	json::JSON& propertiesJson = outJson["Properties"];

	propertiesJson["DiffuseColor"] = json::JSON::Make(json::JSON::Class::Array);
	propertiesJson["DiffuseColor"].append(DiffuseColor.x);
	propertiesJson["DiffuseColor"].append(DiffuseColor.y);
	propertiesJson["DiffuseColor"].append(DiffuseColor.z);
	propertiesJson["DiffuseColor"].append(DiffuseColor.w);

	propertiesJson["DiffuseTexture"] = DiffuseTexturePath;
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

	DiffuseTexturePath = FString(propertiesJson.at("DiffuseTexture").ToString());
}