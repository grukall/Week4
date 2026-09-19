#pragma once
#include "Vector.h"

#include <d3d11.h>
#include "World.h"
#include "Camera.h"
#include "RenderInfo.h"
#include "Gizmo.h"

class AActor;
class FSceneManager;
class URenderer;
struct FRenderTarget2D;
struct FDepthStencil;
class FGraphicsManager;

struct FEditorViewportClient
{
public:
	FEditorViewportClient(URenderer& InRenderer);

	// 이번 프레임에 수집된 픽킹 대상(RenderCollector.PickTargets)만 훑는다.
	// 월드의 액터 계층을 다시 내려가지 않는다.
	// 광선은 ImGui 뷰포트 이미지 기준으로 만든다. 렌더러의 D3D11_VIEWPORT(백버퍼 전체)가 아니다.
	AActor* PerformMousePicking(float perspectiveRatio, const FRenderCollector& RenderCollector, FSceneManager& SceneManager);
	float GetFov() const { return mCamera.mFovDegree; }
	void Update(float deltaTime, FSceneManager* sceneManager, float perspectiveRatio, FRenderCollector& RenderCollector);
	bool IsMouseHit() const { return bMouseHit; }

	void Reset();

	FCamera& GetCamera() { return mCamera; }

	void SetViewportArea(float InLeft, float InTop, float InWidth, float InHeight);
	void ResizeRenderTarget(FGraphicsManager* GraphicsMgr);

	FCamera mCamera;
	FGizmo mGizmo;
	TSharedPtr<FRenderTarget2D> mRenderTarget;
	TSharedPtr<FDepthStencil> mDepthStencil;
	uint32 mWidth = 800;
	uint32 mHeight = 600;
	float mViewportX = 0.0f;
	float mViewportY = 0.0f;
	// 자신의 스플리터 영역 내 상대 위치
	float mViewportLeft = 0.0f;
	float mViewportTop = 0.0f;

private:
	// 선택된 액터의 RenderInfo는 캐시하지 않는다. 필요할 때 ClickedActor->GetRenderInfos()로 그때그때 뽑는다.
	//마우스 밑 무언가가 Actor이면 저장. RayCast 에서 채워야 함 (아직 미구현)
	// INFO: mClickedActor moved to FSceneManager::mSelectedActor.
	//AActor* mClickedActor = nullptr;

	void DeprojectScreenToWorld(int32 MouseX, int32 MouseY,
		float ScreenW, float ScreenH, float NearZ, float FarZ,
		FVector& OutNearPoint, FVector& OutFarPoint);

	void DeprojectScreenToWorldForOrtho(int32 MouseX, int32 MouseY,
		float ScreenW, float ScreenH, float NearZ, float FarZ,
		FVector& OutNearPoint, FVector& OutFarPoint);

	void DeprojectScreenToWorldForUnified(int32 MouseX, int32 MouseY,
		float ScreenW, float ScreenH, float NearZ, float FarZ,
		float orthoDistance, float perspectiveRatio,
		FVector& OutNearPoint, FVector& OutFarPoint
	);

	bool bMouseHit = false;
	
	// RayCast가 이번 프레임에 쏜 광선. 기즈모 드래그가 같은 광선을 다시 쓴다
	FVector mRayNear;
	FVector mRayFar;
};
