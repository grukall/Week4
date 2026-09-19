#include "FStatManager.h"
#include "LaunchEngineLoop.h"
#include "GlobalFNames.h"
#include "MathUtility.h"

FStatManager& FStatManager::Get()
{
	return *GEngineLoop.GetStatManager();
}

void FStatManager::Register(const FName& Name, EStatType Type)
{
	if (Stats.Contains(Name))
	{
		return;
	}

	FStatEntry Entry;
	Entry.Type = Type;
	Entry.Order = NextOrder++;
	// 메모리는 총량이라 항상 수집해야 한다. bEnabled는 "표시 여부"로만 쓴다.
	Stats.Add(Name, Entry);
}



void FStatManager::RefreshEnabled()
{
	const bool bUnit = StatCommands[Name_UNIT];
	const bool bFps = StatCommands[Name_FPS];
	const bool bMemory = StatCommands[Name_MEMORY];

	for (auto& Pair : Stats)
	{
		FStatEntry& Entry = Pair.second;

		switch (Entry.Type)
		{
		case EStatType::Cycle:
			// stat fps는 Frame 하나만 필요하다. stat unit은 전부 필요하다.
			// 둘 중 하나라도 요구하면 켠다.
			Entry.bEnabled = bUnit || (bFps && Pair.first == Name_Frame);
			break;

		case EStatType::Counter:
			Entry.bEnabled = bUnit;
			break;

		case EStatType::Memory:
			// 메모리는 항상 수집된다. 여기서는 표시 여부만 정한다.
			Entry.bEnabled = bMemory;
			break;
		}
	}
}

void FStatManager::Accumulate(const FName& Name, double Value)
{
	if (FStatEntry* Entry = Stats.Find(Name))
	{
		Entry->Accum += Value;
		Entry->Calls++;
	}
}

void FStatManager::ResetFrame()
{
	for (auto& Pair : Stats)
	{
		FStatEntry& Entry = Pair.second;

		Entry.Avg = Entry.Avg * 0.9 + Entry.Accum * 0.1;
		Entry.Max = (Entry.Type == EStatType::Memory)
			? FPlatformMath::Max(Entry.Max, Entry.Accum)
			: FPlatformMath::Max(Entry.Max * 0.995, Entry.Accum);

		// 이번 프레임에 완성된 값을 표시용으로 넘긴다. HUD는 이걸 읽는다.
		// 평활은 시간에만. 개수와 바이트는 정확한 값이라 평균을 내면 안 된다.
		Entry.Display = (Entry.Type == EStatType::Cycle) ? Entry.Avg : Entry.Accum;
		Entry.DisplayCalls = Entry.Calls;

		// 메모리는 현재 총량이므로 리셋하지 않는다.
		if (Entry.Type == EStatType::Memory)
		{
			continue;
		}

		Entry.Accum = 0.0;
		Entry.Calls = 0;
	}
}
