#pragma once

#include <cassert>
#include <unordered_set>

#include "Core.h"
#include "TArray.h"

template <typename T>
class TSet
{
public:
	using SetType = std::unordered_set<T>;
	using Iterator = typename SetType::iterator;

	TSet() = default;
	~TSet() = default;

	Iterator begin() {
		return mSet.begin();
	}

	Iterator end() {
		return mSet.end();
	}

	typename SetType::const_iterator begin() const {
		return mSet.cbegin();
	}

	typename SetType::const_iterator end() const {
		return mSet.cend();
	}

	bool Add(const T& data);

	// Returns removed count
	int32 Remove(const T& data);

	uint32 Num() const;
	void Reset();

	bool Contains(const T& data) const;
	bool IsEmpty() const;
	void Reserve(int32 capacity);

	// 순회 중에 원소가 추가/삭제될 수 있는 곳에서는 이걸로 복사해 두고 배열을 돈다.
	TArray<T> ToTArray() const;

	/*
	void Empty(int32 ExpectedNumElements = 0)
	*/

private:
	SetType mSet;
};

template<typename T>
inline bool TSet<T>::Add(const T& data)
{
	return mSet.insert(data).second;
}

template<typename T>
inline int32 TSet<T>::Remove(const T& data)
{
	return mSet.erase(data);
}

template<typename T>
inline uint32 TSet<T>::Num() const
{
	return static_cast<uint32>(mSet.size());
}

template<typename T>
inline void TSet<T>::Reset()
{
	mSet.clear();
}

template<typename T>
inline bool TSet<T>::Contains(const T& data) const
{
	return mSet.find(data) != mSet.end() ? true : false;
}

template<typename T>
inline bool TSet<T>::IsEmpty() const
{
	return mSet.size() == 0 ? true : false;
}

template<typename T>
inline void TSet<T>::Reserve(int32 capacity)
{
	mSet.reserve(capacity);
}


template<typename T>
inline TArray<T> TSet<T>::ToTArray() const
{
	TArray<T> result;
	result.Reserve(Num());
	for (const T& element : mSet)
	{
		result.Add(element);
	}
	return result;
}
