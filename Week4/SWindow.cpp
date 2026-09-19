#include "SWindow.h"

void SWindow::Resize(const FRect& NewRect)
{
    Rect = NewRect;
    // TODO: 여기서 뷰포트와 연결된 D3D12/OpenGL 렌더 타겟의 크기를 변경하는 로직 호출
}

SWindow* SWindow::GetHoveredWindow(const FPoint& Coord)
{
    if (Rect.Contains(Coord))
    {
        return this; // 클릭된 것이 자기 자신임을 반환
    }
    return nullptr;
}
