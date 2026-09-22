#pragma once

#include "Json/json.hpp"
#include "Vector.h"
#include "Rotator.h"
#include "enum.h"

json::JSON FVectorToJson(const FVector& Vector);
json::JSON FRotatorToJson(const FRotator& Rotator);
json::JSON FVector4ToJson(const FVector4& Vector);

FVector FVectorFromJson(const json::JSON& json);
FRotator FRotatorFromJson(const json::JSON& json);
FVector4 FVector4FromJson(const json::JSON& json);

// 길이가 맞는 숫자 배열인지 확인한다. 각 DeserializeClass가 같은 검사를 반복하고 있어서 묶었다.
bool IsJsonNumberArray(const json::JSON& json, int32 expectedLength);

// 값이 없거나 타입이 다르면 기본값을 그대로 둔다. 옛 씬 파일에 없던 키가 계속 늘어나므로,
// "있으면 읽고 없으면 기본값"을 한 줄로 쓸 수 있어야 한다.
void ReadJsonFloat(const json::JSON& propertiesJson, const char* key, float& outValue);
void ReadJsonInt(const json::JSON& propertiesJson, const char* key, int32& outValue);
void ReadJsonBool(const json::JSON& propertiesJson, const char* key, bool& outValue);
void ReadJsonVector4(const json::JSON& propertiesJson, const char* key, FVector4& outValue);
