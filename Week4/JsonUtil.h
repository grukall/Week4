#pragma once

#include "Json/json.hpp"
#include "Vector.h"
#include "Rotator.h"
#include "enum.h"

json::JSON FVectorToJson(const FVector& Vector);
json::JSON FRotatorToJson(const FRotator& Rotator);
json::JSON EPrimitiveToJson(const EPrimitive& Primitive);

FVector FVectorFromJson(const json::JSON& json);
FRotator FRotatorFromJson(const json::JSON& json);
EPrimitive EPrimitiveFromJson(const json::JSON& json);
