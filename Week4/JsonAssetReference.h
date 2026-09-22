#pragma once

#include "FAssetManager.h"
#include "FGuid.h"
#include "FName.h"
#include "Json/json.hpp"
#include "UAsset.h"

// 씬 파일에 에셋을 적고 되읽는 공통 규칙.
//
// 참조는 GUID와 이름을 같이 적는다. GUID가 정체성이라 에셋을 옮기거나 이름을 바꿔도 따라가지만,
// 엔진 내장 프리미티브처럼 구운 파일이 없는 에셋은 GUID가 비어 있어 이름이 유일한 단서다.
// 참조가 없으면(널 포인터) JSON null로 적어서, "저장 안 됨"과 "비어 있음"을 구분한다.

inline json::JSON AssetReferenceToJson(const UAsset* Asset)
{
	if (!Asset)
	{
		return json::JSON::Make(json::JSON::Class::Null);
	}

	json::JSON referenceJson = json::JSON::Make(json::JSON::Class::Object);
	referenceJson["Guid"] = Asset->GetAssetGuid().ToString().CStr();
	referenceJson["Name"] = Asset->GetAssetName().ToString().CStr();
	return referenceJson;
}

template <typename T>
T* AssetReferenceFromJson(const json::JSON& referenceJson)
{
	if (referenceJson.JSONType() != json::JSON::Class::Object)
	{
		return nullptr;
	}

	FGuid Guid;
	if (referenceJson.hasKey("Guid") && referenceJson.at("Guid").JSONType() == json::JSON::Class::String)
	{
		FGuid::Parse(FString(referenceJson.at("Guid").ToString()), Guid);
	}

	FName AssetName;
	if (referenceJson.hasKey("Name") && referenceJson.at("Name").JSONType() == json::JSON::Class::String)
	{
		AssetName = FName(FString(referenceJson.at("Name").ToString()));
	}

	if (!Guid.IsValid() && AssetName.ToString().Len() == 0)
	{
		return nullptr;
	}

	return FAssetManager::Get().ResolveAssetReferenceAs<T>(Guid, AssetName, true);
}

// 프로퍼티에 적힌 에셋 참조를 읽어 대입한다.
// 키가 없으면(옛 씬 파일) 손대지 않고, 명시적으로 null이면 비운다.
// 참조는 있는데 못 찾으면 기존 값을 그대로 둔다 — 못 찾았다고 비우면 물체가 통째로 사라진다.
template <typename T>
bool ReadJsonAssetReference(const json::JSON& propertiesJson, const char* key, T*& outAsset)
{
	if (!propertiesJson.hasKey(key))
	{
		return false;
	}

	const json::JSON& referenceJson = propertiesJson.at(key);

	if (referenceJson.JSONType() == json::JSON::Class::Null)
	{
		outAsset = nullptr;
		return true;
	}

	if (T* Asset = AssetReferenceFromJson<T>(referenceJson))
	{
		outAsset = Asset;
		return true;
	}

	return false;
}
