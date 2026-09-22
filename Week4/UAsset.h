#pragma once

#include "Core.h"
#include "FGuid.h"
#include "FName.h"
#include "Object.h"

class URenderer;

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
	inline void MarkDirty(bool bDirty = true) { bIsDirty = bDirty; }
	inline const bool IsDirty() const { return bIsDirty; }

	// 이 에셋을 구울 때 쓴 원본(obj/mtl 등)의 GUID. 원본 옆의 .meta 사이드카에 적혀 있는 값이다.
	// 경로가 아니라 GUID라서, 원본을 다른 폴더로 옮기거나 이름을 바꿔도 같은 원본으로 인식된다.
	// 굽지 않은 원본이 없는 에셋(엔진 내장 프리미티브 등)은 비어 있다.
	inline const FGuid& GetImportSourceGuid() const { return ImportSourceGuid; }
	inline void SetImportSourceGuid(const FGuid& InGuid) { ImportSourceGuid = InGuid; }

	// 이 에셋의 영구 식별자. 경로/이름이 바뀌어도 따라가는 유일한 값이라, 다른 에셋이
	// 이걸 참조로 저장한다. 굽기 직전에 FAssetRegistry::AcquireGuidForBake로 채워주면 되고,
	// 한 번 발급된 뒤에는 재임포트를 해도 절대 새로 발급하지 않는다(발급하면 참조가 끊긴다).
	inline const FGuid& GetAssetGuid() const { return AssetGuid; }
	inline void SetAssetGuid(const FGuid& InGuid) { AssetGuid = InGuid; }

	// .uasset 맨 앞에 붙는 고정 헤더. 필드를 넣거나 순서를 바꾸면 Version을 올려야 하고,
	// FAssetRegistry::ReadAssetHeader도 같이 고쳐야 한다.
	static constexpr uint32 AssetFileMagic = 0x54534155;	// 'UAST' (리틀엔디언)
	// 2: UStaticMesh가 머티리얼 참조를 FName 키 대신 FGuid로 저장하기 시작했다.
	// 3: 원본 경로 문자열(AssetPath)을 원본의 GUID(ImportSourceGuid)로 교체했다.
	// 4: UMaterial이 텍스처 참조를 경로 문자열 대신 FGuid로 저장한다. UTexture2D도 굽기 대상이 됐다.
	static constexpr uint32 AssetFileVersion = 4;

	virtual void Serialize(FArchive& Ar) override
	{
		UObject::Serialize(Ar);

		// 포맷이 바뀐 옛날 .uasset을 그대로 읽으면 엉뚱한 길이를 문자열 길이로 해석해서
		// 거대한 할당으로 터진다. 매직과 버전을 맨 앞에 둬서 읽는 쪽이 먼저 걸러내게 한다.
		uint32 Magic = AssetFileMagic;
		uint32 Version = AssetFileVersion;
		Ar << Magic;
		Ar << Version;

		FString ClassName = Ar.IsSaving() ? FString(GetRuntimeClass()->Name) : FString();
		Ar << ClassName;

		Ar << AssetGuid;
		Ar << AssetName;
		Ar << ImportSourceGuid;
	}

	virtual void PostLoad(URenderer* Renderer) {}

protected:
	FName AssetName;
	FGuid AssetGuid;
	FGuid ImportSourceGuid;
	bool bIsDirty = false;
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
