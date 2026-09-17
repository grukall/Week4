#pragma once

#include "core.h"
#include "Vector.h"

enum class EPropertyType { Unknown, Float, Int, String, Bool, Vector, Vector4, WString, Asset};

template <typename T>
struct TPropertyTypeTraits
{
    static_assert(sizeof(T) == 0, "Not Valid Type. Please register it using DEFINE_PROPERTY_TYPE.");
    static constexpr EPropertyType Value = EPropertyType::Unknown;
};

template <typename T>
struct TPropertyTypeTraits<TSharedPtr<T>>
{
    static constexpr EPropertyType Value = EPropertyType::Asset;
};

#define DEFINE_PROPERTY_TYPE(CppType, EnumValue)            \
    template <> struct TPropertyTypeTraits<CppType>         \
    {                                                       \
        static constexpr EPropertyType Value = EPropertyType::EnumValue; \
    };

DEFINE_PROPERTY_TYPE(int, Int)
DEFINE_PROPERTY_TYPE(uint32, Int)
DEFINE_PROPERTY_TYPE(float, Float)
DEFINE_PROPERTY_TYPE(bool, Bool)
DEFINE_PROPERTY_TYPE(FString, String)
DEFINE_PROPERTY_TYPE(FVector, Vector)
DEFINE_PROPERTY_TYPE(FVector4, Vector4)
DEFINE_PROPERTY_TYPE(std::wstring, WString)

template <typename T>
constexpr EPropertyType GetPropertyType()
{
    return TPropertyTypeTraits<T>::Value;
}

struct FProperty
{
    FString Name;
    EPropertyType Type;
    size_t Offset;
    size_t Size;
};