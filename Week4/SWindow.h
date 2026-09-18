#pragma once
#include <algorithm>
#include "FLogManager.h"

struct FPoint
{
    float X;
    float Y;
};

struct FRect
{
    float Left;
    float Top;
    float Right;
    float Bottom;

    float GetWidth() const { return Right - Left; }
    float GetHeight() const { return Bottom - Top; }

    // 점이 사각형 내부에 있는지 판별
    bool Contains(const FPoint& Point) const
    {
        return (Point.X >= Left && Point.X <= Right && Point.Y >= Top && Point.Y <= Bottom);
    }
};

enum class ESplitterDragMode
{
    None,
    VerticalLine,   // 세로선(좌우로 이동) 드래그 중
    HorizontalLine, // 가로선(상하로 이동) 드래그 중
    CenterCross     // 중앙 십자점(자유 이동) 드래그 중
};

class SWindow
{
public:
    FRect Rect;

    virtual ~SWindow() = default;

    virtual void Resize(const FRect& NewRect);

    // 현재 마우스 좌표에 있는 가장 밑단(Leaf) 뷰포트를 찾아 반환
    virtual SWindow* GetHoveredWindow(const FPoint& Coord);
};

class SSplitterQuad : public SWindow
{
public:
    SWindow* TopLeft = nullptr;
    SWindow* BottomLeft = nullptr;
    SWindow* TopRight = nullptr;
    SWindow* BottomRight = nullptr;

    // 중앙 교차점의 비율 (기본 정중앙 50%, 50%)
    float SplitRatioX = 0.5f;
    float SplitRatioY = 0.5f;

    const float HitThickness = 4.0f;
    const float CenterHitRadius = 4.0f;

    ESplitterDragMode DragMode = ESplitterDragMode::None;

public:
    virtual ~SSplitterQuad()
    {
        if (TopLeft) delete TopLeft;
        if (BottomLeft) delete BottomLeft;
        if (TopRight) delete TopRight;
        if (BottomRight) delete BottomRight;
    }

    virtual void Resize(const FRect& NewRect) override
    {
        // 1. 내 영역 갱신
        Rect = NewRect;

        // 2. 가로(X)와 세로(Y)를 자를 기준선 계산
        float SplitX = Rect.Left + (Rect.GetWidth() * SplitRatioX);
        float SplitY = Rect.Top + (Rect.GetHeight() * SplitRatioY);

        // 3. 4개의 뷰포트 영역 계산 및 크기 갱신 연쇄 호출
        if (TopLeft)
        {
            TopLeft->Resize({ Rect.Left, Rect.Top, SplitX, SplitY });
        }
        if (TopRight)
        {
            TopRight->Resize({ SplitX, Rect.Top, Rect.Right, SplitY });
        }
        if (BottomLeft)
        {
            BottomLeft->Resize({ Rect.Left, SplitY, SplitX, Rect.Bottom });
        }
        if (BottomRight)
        {
            BottomRight->Resize({ SplitX, SplitY, Rect.Right, Rect.Bottom });
        }
    }

    virtual SWindow* GetHoveredWindow(const FPoint& Coord) override
    {
        if (!Rect.Contains(Coord)) return nullptr;

        if (TopLeft) { SWindow* Hit = TopLeft->GetHoveredWindow(Coord); if (Hit) return Hit; }
        if (TopRight) { SWindow* Hit = TopRight->GetHoveredWindow(Coord); if (Hit) return Hit; }
        if (BottomLeft) { SWindow* Hit = BottomLeft->GetHoveredWindow(Coord); if (Hit) return Hit; }
        if (BottomRight) { SWindow* Hit = BottomRight->GetHoveredWindow(Coord); if (Hit) return Hit; }

        return this; // 십자 경계선을 클릭한 경우
    }

    // 마우스가 분할선 위에 있는지 확인
    ESplitterDragMode HitTestSplitter(const FPoint& MousePos)
    {
        float SplitX = Rect.Left + (Rect.GetWidth() * SplitRatioX);
        float SplitY = Rect.Top + (Rect.GetHeight() * SplitRatioY);

        bool bHitCenterX = std::abs(MousePos.X - SplitX) <= CenterHitRadius;
        bool bHitCenterY = std::abs(MousePos.Y - SplitY) <= CenterHitRadius;

        if (bHitCenterX && bHitCenterY)
        {
            return ESplitterDragMode::CenterCross;
        }

        bool bHitX = std::abs(MousePos.X - SplitX) <= HitThickness;
        bool bHitY = std::abs(MousePos.Y - SplitY) <= HitThickness;

        if (bHitX) return ESplitterDragMode::VerticalLine;
        if (bHitY) return ESplitterDragMode::HorizontalLine;

        return ESplitterDragMode::None;
    }

    // 마우스 클릭 (드래그 시작)
    void OnMouseDown(const FPoint& MousePos)
    {
        DragMode = HitTestSplitter(MousePos);

        switch (DragMode)
        {
        case ESplitterDragMode::CenterCross:
            UE_LOG("Hit: Center Cross (정중앙 교차점 클릭)");
            break;
        case ESplitterDragMode::VerticalLine:
            UE_LOG("Hit: Vertical Line (세로선 클릭 - 좌우 드래그 가능)");
            break;
        case ESplitterDragMode::HorizontalLine:
            UE_LOG("Hit: Horizontal Line (가로선 클릭 - 상하 드래그 가능)");
            break;
        case ESplitterDragMode::None:
            UE_LOG("Hit: None (선 밖의 영역 클릭)"); 
            break;
        }
    }

    // 마우스 드래그 중 (비율 갱신 및 재렌더링)
    void OnMouseMove(const FPoint& MousePos)
    {
        if (DragMode == ESplitterDragMode::None) return;

        // 드래그 중일 경우 픽셀 좌표를 비율(0.0 ~ 1.0)로 변환
        if (DragMode == ESplitterDragMode::CenterCross || DragMode == ESplitterDragMode::VerticalLine)
        {
            SplitRatioX = (MousePos.X - Rect.Left) / Rect.GetWidth();
            SplitRatioX = std::clamp(SplitRatioX, 0.05f, 0.95f); // 5% ~ 95% 제한
        }

        if (DragMode == ESplitterDragMode::CenterCross || DragMode == ESplitterDragMode::HorizontalLine)
        {
            SplitRatioY = (MousePos.Y - Rect.Top) / Rect.GetHeight();
            SplitRatioY = std::clamp(SplitRatioY, 0.05f, 0.95f);
        }

        // 비율이 바뀌었으므로 즉시 자식 뷰포트들의 크기 재계산
        Resize(Rect);
    }

    // 마우스 떼기 (드래그 종료)
    void OnMouseUp()
    {
        DragMode = ESplitterDragMode::None;
    }
};