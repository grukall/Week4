#pragma once

#include <cstdio>
#include <random>
#include <type_traits>

#include "Core.h"

// 에셋의 영구 식별자. 구울 때 한 번 발급되고, 파일이 옮겨지거나 이름이 바뀌어도 따라간다.
// 경로는 "지금 어디 있나"라는 가변 정보일 뿐이고, 정체성은 이 값이다.
//
// FArchive의 기본 operator<<가 표준 레이아웃 타입을 통째로 memcpy하므로,
// POD로 유지하는 한 직렬화 오버로드를 따로 만들 필요가 없다. 멤버를 늘리거나
// 가상 함수를 붙이면 그 전제가 깨지니 아래 static_assert가 막아준다.
struct FGuid
{
	uint32 A = 0;
	uint32 B = 0;
	uint32 C = 0;
	uint32 D = 0;

	// 전부 0인 값은 "발급된 적 없음"을 뜻한다. 기본 생성된 FGuid가 그 상태다.
	bool IsValid() const
	{
		return (A | B | C | D) != 0;
	}

	bool operator==(const FGuid& Other) const
	{
		return A == Other.A && B == Other.B && C == Other.C && D == Other.D;
	}

	bool operator!=(const FGuid& Other) const
	{
		return !(*this == Other);
	}

	static FGuid NewGuid()
	{
		// 스레드마다 독립된 엔진을 둔다 — 굽기가 메인 스레드 밖에서 불려도 같은 값이 안 나오게.
		thread_local std::mt19937_64 Engine(std::random_device{}());
		std::uniform_int_distribution<uint64> Distribution;

		const uint64 High = Distribution(Engine);
		const uint64 Low = Distribution(Engine);

		FGuid Result;
		Result.A = static_cast<uint32>(High >> 32);
		Result.B = static_cast<uint32>(High & 0xFFFFFFFFull);
		Result.C = static_cast<uint32>(Low >> 32);
		Result.D = static_cast<uint32>(Low & 0xFFFFFFFFull);

		// 0은 "없음"으로 예약돼 있다. 확률은 2^-128이지만 의미가 뒤집히는 값이라 막아둔다.
		if (!Result.IsValid())
		{
			Result.A = 1;
		}

		return Result;
	}

	// 로그와 .meta 사이드카에 쓰는 32자리 16진수 표현.
	FString ToString() const
	{
		char Buffer[33] = {};
		std::snprintf(Buffer, sizeof(Buffer), "%08X%08X%08X%08X", A, B, C, D);

		return FString(Buffer);
	}

	static bool Parse(const FString& InString, FGuid& OutGuid)
	{
		if (InString.Len() != 32)
		{
			return false;
		}

		// sscanf는 MSVC에서 경고를 띄우고, 실패 지점을 알려주지 않는다. 32자를 직접 훑는다.
		uint32 Values[4] = {};
		const char* Cursor = InString.CStr();

		for (int32 Word = 0; Word < 4; ++Word)
		{
			uint32 Accumulated = 0;

			for (int32 Digit = 0; Digit < 8; ++Digit)
			{
				const char Character = *Cursor++;
				uint32 Nibble = 0;

				if (Character >= '0' && Character <= '9')
				{
					Nibble = static_cast<uint32>(Character - '0');
				}
				else if (Character >= 'A' && Character <= 'F')
				{
					Nibble = static_cast<uint32>(Character - 'A') + 10;
				}
				else if (Character >= 'a' && Character <= 'f')
				{
					Nibble = static_cast<uint32>(Character - 'a') + 10;
				}
				else
				{
					return false;
				}

				Accumulated = (Accumulated << 4) | Nibble;
			}

			Values[Word] = Accumulated;
		}

		OutGuid.A = Values[0];
		OutGuid.B = Values[1];
		OutGuid.C = Values[2];
		OutGuid.D = Values[3];

		return true;
	}
};

static_assert(std::is_standard_layout_v<FGuid>, "FGuid must stay standard layout — FArchive serializes it with a raw memcpy.");
static_assert(sizeof(FGuid) == 16, "FGuid size is part of the .uasset format.");

struct FGuidHasher
{
	size_t operator()(const FGuid& Key) const noexcept
	{
		uint64 High = (static_cast<uint64>(Key.A) << 32) | static_cast<uint64>(Key.B);
		const uint64 Low = (static_cast<uint64>(Key.C) << 32) | static_cast<uint64>(Key.D);

		High ^= Low + 0x9e3779b97f4a7c15ull + (High << 6) + (High >> 2);

		return static_cast<size_t>(High);
	}
};
