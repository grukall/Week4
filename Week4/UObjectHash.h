#pragma once

/*
UObject가 생성, 삭제마다 해시 맵으로 UClass* 대응하는 인스턴스들의 포인터들을 관리한다.
엔진이 내부적으로 객체를 찾거나 가비지 컬렉션(GC)을 수행할 때 핵심 역할을 한다.

구조는 간단히 보면 이렇게 되어 있다.

- TMap<UClass*, FHashBucket> ClassToObjectListMap;
- TMap < UClass*, TArray<UClass*> ClassToChildListMap;

두 해시맵은 각각 GUOBjectArray를 모두 순회하는 비용과 모든 UClass를 참조하여 클래스간 상속 관계를 파악하는 비용을 획기적으로 줄일 수 있다.

또한 두 해시 맵을 조합하여 사용하면 다양한 상황에 대응할 수 있다.
1. 정확히 Class가 UClass * 인 인스턴스들이 필요한 경우 → ClassToObjectListMap에서 꺼내 사용
2. Class와 Class를 상속받은 클래스 인스턴스들이 필요한 경우 → ClassToChildListMap의 TArray를 순회하며 ClassToObjectListMap에서 모든 인스턴스들을 꺼내 합쳐서 사용
*/

#include "TMap.h"
#include "TSet.h"
#include "TArray.h"
#include "Object.h"
#include <cassert>

class FUObjectHashTables
{
public:
	static FUObjectHashTables& Get()
	{
		static FUObjectHashTables Singleton;
		return Singleton;
	}

	TMap<const FClassInfo*, TSet<UObject*>> ClassToObjectListMap;
	TMap<const FClassInfo*, TArray<const FClassInfo*>> ClassToChildListMap;
	TSet<const FClassInfo*> RegisteredClasses;

	void Add(UObject* Obj)
	{
		const FClassInfo* ClassInfo = Obj->GetRuntimeClass();
		assert(ClassInfo);

		// 처음 보는 클래스면 상속 체인을 한 번만 등록한다
		if (RegisteredClasses.Add(ClassInfo))   // TSet::Add는 새로 넣었을 때 true
		{
			for (const FClassInfo* Cur = ClassInfo; Cur->SuperClass; Cur = Cur->SuperClass)
			{
				ClassToChildListMap[Cur->SuperClass].Add(Cur);
				if (!RegisteredClasses.Add(Cur->SuperClass)) break;   // 조상은 이미 등록됨
			}
		}

		ClassToObjectListMap[ClassInfo].Add(Obj);
	}

	void Remove(UObject* Obj)
	{
		const FClassInfo* ClassInfo = Obj->GetRuntimeClass();
		if (TSet<UObject*>* Objects = ClassToObjectListMap.Find(ClassInfo))
		{
			Objects->Remove(Obj);
		}
	}
};


// 선언을 먼저 둔다. GetObjectsOfClass가 아래 정의보다 앞서 호출한다.
inline void RecursivelyPopulateDerivedClasses(const FClassInfo* ClassToLookFor, TArray<const FClassInfo*>& ClassesToSearch);

//TODO : 후에 Flag로 순회 오브젝트 목록을 제외 가능하게 한다.(예 : StandAlone시, 에디터용 UObject를 순회 목록에서 제외)
//void GetObjectsOfClass(const FClassInfo* ClassToLookFor, TArray<UObject*>& Results, bool bIncludeDerivedClasses, EObjectFlags ExclusionFlags, EInternalObjectFlags ExclusionInternalFlags)
inline void GetObjectsOfClass(const FClassInfo* ClassToLookFor, TArray<UObject*>& Results, bool bIncludeDerivedClasses)
{
	//TODO : 프로파일러 통계용 매크로 추가
	//SCOPE_CYCLE_COUNTER(STAT_Hash_GetObjectsOfClass);

	FUObjectHashTables& Tables = FUObjectHashTables::Get();
	TArray<const FClassInfo*> ClassesToSearch;
	ClassesToSearch.Emplace(ClassToLookFor);

	if (bIncludeDerivedClasses)
	{
		RecursivelyPopulateDerivedClasses(ClassToLookFor, ClassesToSearch);
	}

	for (const FClassInfo* ClassInfo : ClassesToSearch)
	{
		if (const TSet<UObject*>* Objects = Tables.ClassToObjectListMap.Find(ClassInfo))
		{
			Results.Append(Objects->ToTArray());

		}
	}

	assert(Results.Num() <= UObject::GUObjectArray.Num());
}


inline void RecursivelyPopulateDerivedClasses(const FClassInfo * ClassToLookFor, TArray<const FClassInfo*> &ClassesToSearch)
{
	FUObjectHashTables& Tables = FUObjectHashTables::Get();
	int32 SearchIndex = 0;
	// 시작 클래스는 호출자가 이미 넣어 두었다. 여기서 또 넣으면 결과가 중복된다.
	while (1)
	{
		TArray<const FClassInfo*>* ChildArray = Tables.ClassToChildListMap.Find(ClassesToSearch[SearchIndex]);
		if (ChildArray)
		{
			ClassesToSearch.Reserve(ClassesToSearch.Num() + ChildArray->Num());
			for (const FClassInfo* ChildClass : *ChildArray)
			{
				//if (ExclusionFlags == EInternalObjectFlags::None || !ChildClass->HasAnyInternalFlags(ExclusionFlags))
				//{
				ClassesToSearch.Add(ChildClass);
				//}
			}
		}

		// Now search at next index, if it has been filled in by code above
		SearchIndex++;

		if (SearchIndex >= ClassesToSearch.Num())
		{
			return;
		}
	}
}