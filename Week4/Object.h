#pragma once

#include <functional>

#include "Core.h"
#include "TArray.h"
#include "TSparseArray.h"
#include "ObjectFactory.h"
#include "Property.h"
#include "Archive.h"

namespace json { class JSON; }

class UObject;
class FCamera;

// 씬 로드가 끝난 뒤 런타임 전용 상태를 다시 묶을 때 넘어오는 문맥.
// 카메라나 폰트 아틀라스처럼 파일에 담을 수 없는 것들은 여기서 복원한다.
// 필드가 필요해지면 여기에 추가한다 — 훅의 시그니처를 매번 바꾸지 않기 위한 그릇이다.
struct FSceneLoadContext
{
	FCamera* Camera = nullptr;
};

//using ConstructorFunc = UObject * (*)();

struct FClassInfo
{
	FString Name;
	const FClassInfo* SuperClass;
	std::function<UObject* ()> Constructor;

	TArray<FProperty> Properties;
	inline const TArray<FProperty>& GetProperties() const { return Properties; }

	FClassInfo(FString name, const FClassInfo* superClass, std::function<UObject* ()> constructor)
		: Name(std::move(name)), SuperClass(superClass), Constructor(constructor) {
	}

	UObject* CreateInstance() const;

	bool IsChildOf(const FClassInfo* other) const;

	template <typename T>
	void AddProperty(const FString& InName, uint64 InOffset)
	{
		FString WidgetId("##");
		WidgetId.Append(InName);

		Properties.Add({ InName, std::move(WidgetId), GetPropertyType<T>(), InOffset, sizeof(T), GetPropertyClassInfo<T>() });
	}

private:
};

struct FObjectID
{
	int32 UUID;
	uint32 InternalIndex;
};

class UObject
{
public:
	// Todo: Fix
	int32 UUID;
	uint32 InternalIndex;

	virtual ~UObject();
	virtual void Destroy();

	void Initialize();

	// StaticClass() in Unreal Engine
	static const FClassInfo* GetClass();

	// GetClass() in Unreal Engine
	inline virtual const FClassInfo* GetRuntimeClass() const { return mClassInfo; }
	 
	// TODO?: Replace json type with a more generic type, such as a variant or a map
	virtual void SerializeClass(json::JSON& outJson) const;
	virtual void DeserializeClass(const json::JSON& inJson);

	// 씬 전체가 복원된 뒤 한 번 불린다. DeserializeClass와 달리 "다른 객체들도 이미 다 있다"가
	// 보장되고, 카메라처럼 파일 밖에서 오는 것도 받을 수 있다.
	// 이 복원을 호출자(예: GUI 버튼 핸들러)에 맡기면 로드 경로마다 빠뜨리게 된다.
	virtual void PostSceneLoad(const FSceneLoadContext& context) {}

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	bool IsA() const;

	bool IsA(const FClassInfo* classInfo) const;

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	TObject* Cast();

	static UObject* GetObjectByUUID(int32 uuid);
	static UObject* GetObjectByInternalIndex(uint32 internalIndex);

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	static TObject* GetObjectByUUID(int32 uuid);

	template<typename TObject>
		requires std::derived_from<TObject, UObject>
	static TObject* GetObjectByInternalIndex(uint32 internalIndex);

	inline static uint64 GetGObjectRevision() { return GUObjectRevision; }

	virtual void Serialize(FArchive& Ar) {}

public:
	static TSparseArray<UObject*> GUObjectArray;

protected:
	UObject();

	//이 값이 변경되었다는 건, UObject 목록에 변경이 있었다는 것
	inline static uint64 GUObjectRevision = 0;

private:

	friend struct FObjectFactory;
	const FClassInfo* mClassInfo = nullptr;
};


#include  "Object.inl"
