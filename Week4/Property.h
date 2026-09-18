#pragma once

#include "core.h"
#include "Vector.h"

struct FClassInfo;

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

// 원시 포인터 프로퍼티(T*)의 특수화는 UAsset.h에 있다.
// 여기서 UAsset.h를 include하면 Property.h -> UAsset.h -> Object.h -> Property.h 로 순환한다.

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

// 에셋 포인터 프로퍼티가 어떤 클래스를 가리키는지 알아내는 트레잇.
// 포인터가 아닌 타입은 nullptr. T* 특수화는 UAsset.h에 있다.
template <typename T>
struct TPropertyClassInfo
{
    static const FClassInfo* Get() { return nullptr; }
};

template <typename T>
const FClassInfo* GetPropertyClassInfo()
{
    return TPropertyClassInfo<T>::Get();
}

struct FProperty
{
    FString Name;
    EPropertyType Type;
    size_t Offset;
    size_t Size;

    // Type이 Asset일 때만 채워진다. 드롭다운에 어떤 에셋을 나열할지 결정한다.
    const FClassInfo* ClassInfo = nullptr;
};