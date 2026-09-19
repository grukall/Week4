#pragma once
#include "Vector.h"

#include <d3d11.h>
#include "World.h"
#include "Camera.h"
#include "RenderInfo.h"
#include "Gizmo.h"
#include "FStatManager.h"

class AActor;
class FSceneManager;
class URenderer;
class UFontAtlas;
class FFontAtlas;

// 스탯 HUD 한 줄. 라벨은 우측 정렬, 값은 좌측 정렬로 두 열을 이룬다.
struct FStatRow
{
	char Label[32] = {};
	char Value[48] = {};
	FVector4 Color = FVector4(1.f, 1.f, 1.f, 1.f);
};

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

	FCamera mCamera;
	FGizmo mGizmo;

private:

	void DrawStatsHUD(FStatManager& StatManager, UFontAtlas* Atlas,
		FRenderCollector& Collector, float ViewportW, float ViewportH);

	void GatherStatFPS(TArray<FStatRow>& Rows);
	void GatherStatUnit(TArray<FStatRow>& Rows);

	void DrawStatMemoryTable(UFontAtlas* Atlas, FRenderCollector& RenderCollector, float ViewportW);

	// 단색 사각형. 표의 줄무늬 배경
	static void DrawStatRect(FRenderCollector& RenderCollector, float X, float Y, float W, float H, const FVector4& Color);

	void DrawStatRows(UFontAtlas* Atlas, FRenderCollector& RenderCollector, const TArray<FStatRow>& Rows, float ViewportW);
	static float MeasureStatText(FFontAtlas* FontAtlas, const char* Text, float Scale);
	static void DrawStatText(UFontAtlas* Atlas, FRenderCollector& RenderCollector,const char* Text, float LeftX, float Y, float Scale, const FVector4& Color);

	// HUD에 그릴 글자 크기(픽셀).
	static constexpr float StatFontPixelSize = 24.0f;

	// 화면 가장자리에서 띄우는 여백. 언리얼도 뷰포트 테두리에 붙이지 않는다.
	static constexpr float StatScreenMargin = 48.0f;

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

	// 스탯 HUD가 화면 크기(=씬 렌더타겟 크기)를 물어보려고 들고 있는다.
	URenderer* mRenderer = nullptr;

	// RayCast가 이번 프레임에 쏜 광선. 기즈모 드래그가 같은 광선을 다시 쓴다
	FVector mRayNear;
	FVector mRayFar;
};
