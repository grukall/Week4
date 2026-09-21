#pragma once
#include <windows.h>

// Windows 10 1803부터 지원. SDK가 낮으면 정의가 없어서 직접 넣는다.
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

class FFrameTimer
{
public:
	FFrameTimer(bool bFrameLimit, int TargetFPS = 120) : targetFrameTime(1000.0 / TargetFPS), elapsedTime(1000.0 / TargetFPS), bIsFrameLimit(bFrameLimit)
	{
		QueryPerformanceFrequency(&Frequency);
		QueryPerformanceCounter(&StartTime);
		EndTime = StartTime;


		// 고분해능 대기 타이머는 약 0.5ms 단위
		WaitTimer = CreateWaitableTimerExW(nullptr, nullptr,
			CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
			TIMER_MODIFY_STATE | SYNCHRONIZE);
	}

	~FFrameTimer()
	{
		if (WaitTimer)
		{
			CloseHandle(WaitTimer);
		}
	}

	FFrameTimer(const FFrameTimer&) = delete;
	FFrameTimer& operator=(const FFrameTimer&) = delete;

	void StartFrame()
	{
		deltaTime = (float)(elapsedTime * 0.001);
		if (deltaTime > 0.1f) deltaTime = 0.1f;

		QueryPerformanceCounter(&StartTime);
	}

	void EndFrame()
	{
		elapsedTime = MeasureElapsedMs();

		if (!bIsFrameLimit)
		{
			return;
		}

		// 1단계: 여유가 있는 동안은 자면서 기다린다. 그만큼 코어를 다른 프로그램에 넘긴다.
		// 타이머가 조금 늦게 깨워도 목표를 넘기지 않도록 SpinMarginMs는 남겨둔다.
		const double SleepUntil = targetFrameTime - SpinMarginMs;
		while (elapsedTime < SleepUntil)
		{
			if (!SleepMs(SleepUntil - elapsedTime))
			{
				break; // 타이머를 못 만들었거나 실패했다. 스핀으로 넘어간다.
			}
			elapsedTime = MeasureElapsedMs();
		}

		// 2단계: 남은 SpinMarginMs 미만은 스핀으로 정확히 맞춘다.
		while (elapsedTime < targetFrameTime)
		{
			// YieldProcessor()는 Windows SDK(winnt.h)가 제공하는 매크로로, x86/x64에서는 CPU의 PAUSE 명령 하나로 확장됩니다 (_mm_pause()와 같음).
			// CPU는 루프 조건을 투기적으로 예측해 QueryPerformanceCounter 호출을 수십 개 미리 밀어 넣습니다.
			// 그러다 조건이 드디어 바뀌면 그 모든 투기 실행이 무효가 되고, 파이프라인을 비우는 데 큰 페널티를 먹습니다.
			// PAUSE는 "여기는 스핀 대기 루프다"라고 CPU에 알려주는 힌트라서, CPU가 투기 실행을 자제하고 파이프라인 비우기 페널티를 피합니다.
			YieldProcessor();
			elapsedTime = MeasureElapsedMs();
		}
	}

	float GetDeltaTime() const { return deltaTime; }
	float GetFPS() const { return elapsedTime > 0.0 ? (float)(1000.0 / elapsedTime) : 0.f; }

private:
	double MeasureElapsedMs()
	{
		QueryPerformanceCounter(&EndTime);
		return (EndTime.QuadPart - StartTime.QuadPart) * 1000.0 / Frequency.QuadPart;
	}

	// 실제로 기다렸으면 true. 타이머를 쓸 수 없으면 false.
	bool SleepMs(double Ms)
	{
		if (!WaitTimer)
		{
			return false;
		}

		// 100ns 단위. 음수는 "지금부터 상대 시간"이라는 뜻이다.
		LARGE_INTEGER DueTime;
		DueTime.QuadPart = -(LONGLONG)(Ms * 10000.0);
		if (DueTime.QuadPart >= 0)
		{
			return false; // 100ns보다 짧다. 잘 필요가 없다.
		}

		if (!SetWaitableTimer(WaitTimer, &DueTime, 0, nullptr, nullptr, FALSE))
		{
			return false;
		}

		return WaitForSingleObject(WaitTimer, INFINITE) == WAIT_OBJECT_0;
	}

	// 타이머가 깨어나는 오차를 흡수할 여유. 이만큼은 스핀으로 채운다.
	static constexpr double SpinMarginMs = 1.0;

	double targetFrameTime;
	double elapsedTime;
	float deltaTime = 0.f;
	bool bIsFrameLimit = true;
	LARGE_INTEGER Frequency, StartTime, EndTime;
	HANDLE WaitTimer = nullptr;
};
