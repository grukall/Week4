#pragma once

#include "Core.h"
#include "FName.h"
#include "Object.h"

class UAsset : public UObject
{
	REFLECT_CLASS(UAsset, UObject)
public:
	UAsset() = default;
	virtual ~UAsset() = default;

	using UObject::Initialize;
	void Initialize(const FName& InAssetName)
	{
		UObject::Initialize();

		AssetName = InAssetName;
	}

	inline const FName& GetAssetName() const { return AssetName; }

protected:
	FName AssetName;
	FString AssetPath;
};

class FAssetSource
{
public:
	virtual ~FAssetSource() = default; 
};

class FAssetLoader
{
public:
	virtual ~FAssetLoader() = default;

	virtual UAsset* LoadAsset(const FName& AssetName, FAssetSource& AssetSource) = 0;
	virtual void UnloadAsset(UAsset* Asset) = 0;
};

// 원시 포인터 프로퍼티는 에셋만 허용한다.
// UAsset이 완전한 타입인 이 자리에 둬야 한다. Property.h에 두면 헤더 순환이 생기고,
// 전방 선언만으로는 is_base_of가 조용히 false가 되어 엉뚱한 곳에서 터진다.
template <typename T>
struct TPropertyTypeTraits<T*>
{
	static_assert(std::is_base_of_v<UAsset, T>,
		"Raw pointer properties are only supported for UAsset-derived types.");
	static constexpr EPropertyType Value = EPropertyType::Asset;
};

// 에셋 포인터 프로퍼티가 가리키는 클래스. 드롭다운 후보를 고르는 데 쓴다.
template <typename T>
struct TPropertyClassInfo<T*>
{
	static const FClassInfo* Get() { return T::GetClass(); }
};
