#pragma once

#include "RenderInfo.h"

class FViewport;
class FGraphicsManager;
class FSceneManager;

class FViewportClient
{
public:
	virtual ~FViewportClient() = default;

	// 뷰포트 렌더링 호출 (FViewport::Draw()에서 호출됨)
	virtual void Draw(FViewport* Viewport, FGraphicsManager* GraphicsMgr, FSceneManager* SceneMgr) = 0;

	// 업데이트 및 틱 처리
	virtual void Update(float DeltaTime, FSceneManager* SceneMgr, float PerspectiveRatio, FRenderCollector& RenderCollector) = 0;
};
