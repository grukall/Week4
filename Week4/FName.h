#pragma once

#include "Core.h"

struct FName
{
	FName();
	FName(const char* pStr);
	FName(const FString& Name);

	int32 Compare(const FName& Other) const;
	bool operator==(const FName& Other) const;

	void ParseName(const FString& InName, FString& OutName, uint32& OutNumber) const;

	inline bool IsValid() const { return DisplayIndex != -1 && ComparisonIndex != -1; }

	FString ToString() const;

	int32 DisplayIndex;
	int32 ComparisonIndex;
	uint32 Number;
};

struct FNameHasher
{
	std::size_t operator()(const FName& Name) const noexcept
	{
		return std::hash<int32>()(Name.ComparisonIndex) ^ std::hash<uint32>()(Name.Number);
	}
};