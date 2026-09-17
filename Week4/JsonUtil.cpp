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

json::JSON EPrimitiveToJson(const EPrimitive& Primitive)
{
	switch (Primitive)
	{
	case EPrimitive::EP_Sphere:
		return json::JSON("Sphere");
	case EPrimitive::EP_Cube:
		return json::JSON("Cube");
	case EPrimitive::EP_Triangle:
		return json::JSON("Triangle");
	case EPrimitive::EP_GizmoArrow:
		return json::JSON("GizmoArrow");
	case EPrimitive::EP_Circle:
		return json::JSON("Circle");
	case EPrimitive::EP_Plane:
		return json::JSON("Plane");
	default:
		throw std::runtime_error("Unknown EPrimitive value");
	}
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

EPrimitive EPrimitiveFromJson(const json::JSON& json)
{
	if (json.JSONType() != json::JSON::Class::String)
	{
		throw std::runtime_error("Json String expected for EPrimitive");
	}
	std::string primitiveStr = json.ToString();
	if (primitiveStr == "Sphere")
	{
		return EPrimitive::EP_Sphere;
	}
	else if (primitiveStr == "Cube")
	{
		return EPrimitive::EP_Cube;
	}
	else if (primitiveStr == "Triangle")
	{
		return EPrimitive::EP_Triangle;
	}
	else if (primitiveStr == "GizmoArrow")
	{
		return EPrimitive::EP_GizmoArrow;
	}
	else if (primitiveStr == "Circle")
	{
		return EPrimitive::EP_Circle;
	}
	else if (primitiveStr == "Plane")
	{
		return EPrimitive::EP_Plane;
	}
	else
	{
		throw std::runtime_error("Unknown EPrimitive value in JSON");
	}
}
