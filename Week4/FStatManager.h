#pragma once

#include "TMap.h"
#include "FName.h"

#define PP_CAT_INNER(A, B) A##B
#define PP_CAT(A, B)       PP_CAT_INNER(A, B)


enum class EStatType : uint8
{
    Cycle,      // 시간(ms). 매 프레임 리셋
    Counter,    // 개수. 매 프레임 리셋
    Memory,     // 바이트. 리셋하지 않고 계속 누적
};

struct FStatEntry
{
    EStatType Type = EStatType::Counter;
    bool bEnabled = false;
    double Accum = 0.0;
    int32 Calls = 0;
    int32 Order = 0;    //표시 정렬
    double Avg = 0.0, Max = 0.0;

    // HUD는 프레임 중간에 그려지므로 Accum은 아직 미완성이다.
    // ResetFrame이 직전 프레임의 완성된 값을 여기로 옮겨두고, 표시는 이쪽을 읽는다.
    // (언리얼도 Stats 스레드 때문에 한 프레임 늦은 값을 보여준다)
    double Display = 0.0;
    int32 DisplayCalls = 0;
};

class FStatManager
{
public:
    static FStatManager& Get();

    // 스탯 이름을 목록에 올린다. 등록돼 있어야 콘솔에서 켤 수 있다.
    void Register(const FName& Name, EStatType Type);

    inline bool IsCollecting(const FName& Name) const
    {
        if (const FStatEntry* Entry = Stats.Find(Name))
        {
            return Entry->bEnabled;
        }

        // No Entry
        return false;
    }

    // StatCommands(어떤 stat 명령이 켜져 있나)로부터 각 스탯의 수집 여부를 다시 계산한다.
    // 명령끼리 같은 스탯을 공유하므로(FPS와 UNIT이 둘 다 Frame을 쓴다) 개별로 켜고 끄면
    // 나중에 끈 쪽이 상대를 덮어쓴다. 항상 전체를 다시 계산해야 한다.
    void RefreshEnabled();

    // 직전 프레임의 완성된 값. 없으면 0.
    double GetDisplay(const FName& Name) const
    {
        const FStatEntry* Entry = Stats.Find(Name);
        return Entry ? Entry->Display : 0.0;
    }

    // Value는 부호 있는 값. 메모리 해제는 음수를 넘긴다.
    void Accumulate(const FName& Name, double Value);
    void ResetFrame(); // 매 프레임 시작 시 Accum/Calls를 0으로 (Memory 제외)

    TMap<FName, FStatEntry, FNameHasher> Stats;
    TMap<FName, bool, FNameHasher> StatCommands;

private:
    int32 NextOrder = 0;
};

class FFrameTimer;

// 언리얼의 DrawStatsHUD에 해당한다. ImGui 창 안에서 호출해야 한다.
// RightX/TopY는 뷰포트 이미지의 우상단 화면 좌표.
void DrawStatsHUD(FStatManager& StatManager, const FFrameTimer& FrameTimer, float RightX, float TopY);

inline double GetMsPerCount()
{
    static const double MsPerCount = []
    {
        LARGE_INTEGER Freq;
        QueryPerformanceFrequency(&Freq);
        return 1000.0 / static_cast<double>(Freq.QuadPart);
    }();
    return MsPerCount;
}

// FName 생성은 문자열 조회라 매 호출하면 손해다. 매크로에서 static으로 한 번만 만든다.
struct FStatId
{
    FStatId(const char* InName, EStatType InType) : Name(InName)
    {
        FStatManager::Get().Register(Name, InType);
    }
    FName Name;
};

struct FScopeCycleCounter
{

    static inline FScopeCycleCounter* Current = nullptr;
    FScopeCycleCounter* Parent = nullptr;
    double ChildMs = 0.0;   // 내 직속 자식들이 먹은 시간

    FScopeCycleCounter(const FName& InName) : Name(InName)
    {
        bActive = FStatManager::Get().IsCollecting(Name);
        if (!bActive) return;
        Parent = Current;
        Current = this;
        QueryPerformanceCounter(&Start);
    }
    ~FScopeCycleCounter()
    {
        if (!bActive) return;
        LARGE_INTEGER End; QueryPerformanceCounter(&End);
        const double Ms = GetMsPerCount();
        FStatManager::Get().Accumulate(Name, (End.QuadPart - Start.QuadPart) * Ms);

        Current = Parent;                                         // 원래대로 복구
        FStatManager::Get().Accumulate(Name, Ms);                 // Inclusive
        FStatManager::Get().Accumulate(Name, -ChildMs);           // Exclusive
        if (Parent) Parent->ChildMs += Ms;                        // 부모에게 "나 이만큼 썼어" 보고

    }

    FName Name; LARGE_INTEGER Start; bool bActive;
};


// Cycle
// 지역 변수가 스코프 끝까지 살아남아야 하므로 do-while로 감싸지 않는다.
#define SCOPE_CYCLE_COUNTER_IMPL(Tag, StatName)                                     \
    static const FStatId PP_CAT(_StatId_, Tag)(StatName, EStatType::Cycle);         \
    FScopeCycleCounter PP_CAT(_StatScope_, Tag)(PP_CAT(_StatId_, Tag).Name)

//스코프를 사용하여 사용 시간 기록
#define SCOPE_CYCLE_COUNTER(StatName) SCOPE_CYCLE_COUNTER_IMPL(__COUNTER__, StatName)

//이미 계산된 ms를 대입해야 하는 경우 사용
#define SET_CYCLE_COUNTER_IMPL(Tag, StatName, Ms)                                \
    do {                                                                         \
        static const FStatId PP_CAT(_StatId_, Tag)(StatName, EStatType::Cycle);  \
        if (FStatManager::Get().IsCollecting(PP_CAT(_StatId_, Tag).Name))        \
            FStatManager::Get().Accumulate(PP_CAT(_StatId_, Tag).Name, (Ms));    \
    } while (0)

//계산된 사용시간을 기록
#define SET_CYCLE_COUNTER(StatName, Ms) SET_CYCLE_COUNTER_IMPL(__COUNTER__, StatName, Ms)

// Counter
// do-while(0)으로 감싸 전체를 문장 하나로 만든다. if/else 안에서도 안전.
#define INC_DWORD_STAT_BY_IMPL(Tag, StatName, N)                                    \
    do {                                                                            \
        static const FStatId PP_CAT(_StatId_, Tag)(StatName, EStatType::Counter);   \
        if (FStatManager::Get().IsCollecting(PP_CAT(_StatId_, Tag).Name))           \
        {                                                                           \
            FStatManager::Get().Accumulate(PP_CAT(_StatId_, Tag).Name,              \
                                           static_cast<double>(N));                 \
        }                                                                           \
    } while (0)

//StatName의 스텟에 카운트 증가
#define INC_DWORD_STAT(StatName)       INC_DWORD_STAT_BY_IMPL(__COUNTER__, StatName, 1)

//StatName의 스텟에 카운트를 N만큼 증가
#define INC_DWORD_STAT_BY(StatName, N) INC_DWORD_STAT_BY_IMPL(__COUNTER__, StatName, N)


// Memory
// 메모리는 "현재 총량"이라 꺼져 있어도 계속 세야 한다. IsCollecting 가드를 걸지 않는다.
#define MEMORY_STAT_BY_IMPL(Tag, StatName, SignedSize)                              \
    do {                                                                            \
        static const FStatId PP_CAT(_StatId_, Tag)(StatName, EStatType::Memory);    \
        FStatManager::Get().Accumulate(PP_CAT(_StatId_, Tag).Name, (SignedSize));   \
    } while (0)

//StatName의 스텟에 사이즈 증가
#define INC_MEMORY_STAT_BY(StatName, Size) \
    MEMORY_STAT_BY_IMPL(__COUNTER__, StatName,  static_cast<double>(Size))

//StatName의 스텟에 사이즈 감소
#define DEC_MEMORY_STAT_BY(StatName, Size) \
    MEMORY_STAT_BY_IMPL(__COUNTER__, StatName, -static_cast<double>(Size))
