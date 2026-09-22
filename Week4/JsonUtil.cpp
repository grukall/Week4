#include "JsonUtil.h"

#include "Json/json.hpp"

json::JSON FVectorToJson(const FVector& Vector)
{
	json::JSON vectorJson = json::JSON::Make(json::JSON::Class::Array);
	vectorJson[0] = Vector.x;
	vectorJson[1] = Vector.y;
	vectorJson[2] = Vector.z;
	return vectorJson;
}

json::JSON FRotatorToJson(const FRotator& Rotator)
{
	json::JSON rotatorJson = json::JSON::Make(json::JSON::Class::Array);
	rotatorJson[0] = Rotator.Pitch;
	rotatorJson[1] = Rotator.Yaw;
	rotatorJson[2] = Rotator.Roll;
	return rotatorJson;
}

FVector FVectorFromJson(const json::JSON& json)
{
	if (json.JSONType() != json::JSON::Class::Array)
	{
		throw std::runtime_error("Json Array expected for FVector");
	}

	return FVector(json.at(0).ToFloat(), json.at(1).ToFloat(), json.at(2).ToFloat());
}

FRotator FRotatorFromJson(const json::JSON& json)
{
	if (json.JSONType() != json::JSON::Class::Array)
	{
		throw std::runtime_error("Json Array expected for FRotator");
	}

	return FRotator(json.at(0).ToFloat(), json.at(1).ToFloat(), json.at(2).ToFloat());
}

json::JSON FVector4ToJson(const FVector4& Vector)
{
	json::JSON vectorJson = json::JSON::Make(json::JSON::Class::Array);
	vectorJson[0] = Vector.x;
	vectorJson[1] = Vector.y;
	vectorJson[2] = Vector.z;
	vectorJson[3] = Vector.w;
	return vectorJson;
}

FVector4 FVector4FromJson(const json::JSON& json)
{
	if (json.JSONType() != json::JSON::Class::Array)
	{
		throw std::runtime_error("Json Array expected for FVector4");
	}

	return FVector4(json.at(0).ToFloat(), json.at(1).ToFloat(), json.at(2).ToFloat(), json.at(3).ToFloat());
}

bool IsJsonNumberArray(const json::JSON& json, int32 expectedLength)
{
	if (json.JSONType() != json::JSON::Class::Array || json.length() != expectedLength)
	{
		return false;
	}

	for (int32 i = 0; i < expectedLength; ++i)
	{
		const json::JSON::Class elementType = json.at(static_cast<unsigned>(i)).JSONType();

		// 정수로 적힌 값도 받아준다. 손으로 고친 씬 파일은 1.0을 그냥 1로 적는다.
		if (elementType != json::JSON::Class::Floating && elementType != json::JSON::Class::Integral)
		{
			return false;
		}
	}

	return true;
}

namespace
{
	// ToFloat()는 타입이 Floating이 아니면 말없이 0을 돌려준다. 정수로 적힌 값을 0으로
	// 읽어버리지 않게 여기서 타입을 보고 갈라준다.
	float JsonNumberToFloat(const json::JSON& json)
	{
		if (json.JSONType() == json::JSON::Class::Integral)
		{
			return static_cast<float>(json.ToInt());
		}

		return static_cast<float>(json.ToFloat());
	}
}

void ReadJsonFloat(const json::JSON& propertiesJson, const char* key, float& outValue)
{
	if (!propertiesJson.hasKey(key))
	{
		return;
	}

	const json::JSON& valueJson = propertiesJson.at(key);
	if (valueJson.JSONType() != json::JSON::Class::Floating && valueJson.JSONType() != json::JSON::Class::Integral)
	{
		return;
	}

	outValue = JsonNumberToFloat(valueJson);
}

void ReadJsonInt(const json::JSON& propertiesJson, const char* key, int32& outValue)
{
	if (!propertiesJson.hasKey(key) || propertiesJson.at(key).JSONType() != json::JSON::Class::Integral)
	{
		return;
	}

	outValue = static_cast<int32>(propertiesJson.at(key).ToInt());
}

void ReadJsonBool(const json::JSON& propertiesJson, const char* key, bool& outValue)
{
	if (!propertiesJson.hasKey(key) || propertiesJson.at(key).JSONType() != json::JSON::Class::Boolean)
	{
		return;
	}

	outValue = propertiesJson.at(key).ToBool();
}

void ReadJsonVector4(const json::JSON& propertiesJson, const char* key, FVector4& outValue)
{
	if (!propertiesJson.hasKey(key) || !IsJsonNumberArray(propertiesJson.at(key), 4))
	{
		return;
	}

	const json::JSON& vectorJson = propertiesJson.at(key);
	outValue = FVector4(
		JsonNumberToFloat(vectorJson.at(0u)),
		JsonNumberToFloat(vectorJson.at(1u)),
		JsonNumberToFloat(vectorJson.at(2u)),
		JsonNumberToFloat(vectorJson.at(3u)));
}