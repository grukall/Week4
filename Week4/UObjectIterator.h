#pragma once

#include "Core.h"
#include "Object.h"
#include "TArray.h"
#include "UObjectHash.h"



class FObjectIteratorBase
{
public:
	enum EEndTagType
	{
		EndTag
	};

	void operator++()
	{
		Advance();
	}

	// 이터레이터가 유효한 원소를 가리키면 true.
	explicit operator bool() const
	{
		return Index >= 0 && Index < ObjectArray.Num();
	}

	bool operator!() const
	{
		return !static_cast<bool>(*this);
	}

	bool operator==(const FObjectIteratorBase& Rhs) const { return Index == Rhs.Index; }
	bool operator!=(const FObjectIteratorBase& Rhs) const { return Index != Rhs.Index; }

protected:
	FObjectIteratorBase()
		: Index(-1)
	{
	}

	explicit FObjectIteratorBase(int32 InIndex)
		: Index(InIndex)
	{
	}

	UObject* GetObject() const
	{
		return ObjectArray[Index];
	}

	// 다음 유효한 원소로 전진한다. 목록에 nullptr이 섞여 있어도 건너뛴다.
	bool Advance()
	{
		while (++Index < ObjectArray.Num())
		{
			if (ObjectArray[Index])
			{
				return true;
			}
		}
		return false;
	}

protected:
	TArray<UObject*> ObjectArray;
	int32 Index;
};

template <typename T>
class TObjectIterator : public FObjectIteratorBase
{
public:
	// T와 그 파생 클래스의 인스턴스를 모두 훑는다.
	explicit TObjectIterator(bool bIncludeDerivedClasses = true)
		: TObjectIterator(T::GetClass(), bIncludeDerivedClasses)
	{
	}

	explicit TObjectIterator(const FClassInfo* ClassInfo, bool bIncludeDerivedClasses = true)
	{
		GetObjectsOfClass(ClassInfo, ObjectArray, bIncludeDerivedClasses);
		Advance();
	}

	// 범위 기반 for의 끝을 나타낸다.
	TObjectIterator(EEndTagType, const TObjectIterator& Begin)
		: FObjectIteratorBase(Begin.ObjectArray.Num())
	{
	}

	T* operator*() const
	{
		return static_cast<T*>(GetObject());
	}

	T* operator->() const
	{
		return static_cast<T*>(GetObject());
	}
};

/*
UObject 전체를 훑을 때는 해시를 거치지 않는다.
*/
template <>
class TObjectIterator<UObject> : public FObjectIteratorBase
{
public:
	explicit TObjectIterator(bool bIncludeDerivedClasses = true)
	{
		ObjectArray = UObject::GUObjectArray.ToTArray();
		Advance();
	}

	TObjectIterator(EEndTagType, const TObjectIterator& Begin)
		: FObjectIteratorBase(Begin.ObjectArray.Num())
	{
	}

	UObject* operator*() const
	{
		return GetObject();
	}

	UObject* operator->() const
	{
		return GetObject();
	}
};

// 범위 기반 for 지원.
template <typename T>
TObjectIterator<T> begin(const TObjectIterator<T>& Iter)
{
	return Iter;
}

template <typename T>
TObjectIterator<T> end(const TObjectIterator<T>& Iter)
{
	return TObjectIterator<T>(FObjectIteratorBase::EndTag, Iter);
}

template <typename T>
struct TObjectRange
{
	explicit TObjectRange(bool bIncludeDerivedClasses = true)
		: Iter(bIncludeDerivedClasses)
	{
	}

	TObjectIterator<T> begin() const { return Iter; }
	TObjectIterator<T> end() const { return TObjectIterator<T>(FObjectIteratorBase::EndTag, Iter); }

private:
	TObjectIterator<T> Iter;
};
