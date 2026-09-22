#pragma once
#include "Vector.h"

#include <d3d11.h>
#include "World.h"
#include "Camera.h"
#include "RenderInfo.h"
#include "Gizmo.h"
#include "FStatManager.h"
#include "enum.h"

#include "SWindow.h"
#include "FViewportClient.h"

class AActor;
class FViewport;
class FSceneManager;
class URenderer;
class UFontAtlas;
class FFontAtlas;
struct FRenderTarget2D;
struct FDepthStencil;
class FGraphicsManager;
struct ImVec2;

enum class EViewportType
{
	Perspective,
	Top,
	Bottom,
	Left,
	Right,
	Front,
	Back
};

// 스탯 HUD 한 줄. 라벨은 우측 정렬, 값은 좌측 정렬로 두 열을 이룬다.
struct FStatRow
{
	char Label[32] = {};
	char Value[48] = {};
	FVector4 Color = FVector4(1.f, 1.f, 1.f, 1.f);
};

struct FEditorViewportClient : public FViewportClient
{
public:
	FEditorViewportClient(URenderer& InRenderer);

	// 이번 프레임에 수집된 픽킹 대상(RenderCollector.PickTargets)만 훑는다.
	// 월드의 액터 계층을 다시 내려가지 않는다.
	// 광선은 ImGui 뷰포트 이미지 기준으로 만든다. 렌더러의 D3D11_VIEWPORT(백버퍼 전체)가 아니다.
	AActor* PerformMousePicking(float perspectiveRatio, const FRenderCollector& RenderCollector, FSceneManager& SceneManager);
	float GetFov() const { return mCamera.mFovDegree; }
	virtual void Update(float deltaTime, FSceneManager* sceneManager, float perspectiveRatio, FRenderCollector& RenderCollector) override;
	bool IsMouseHit() const { return bMouseHit; }

	void Reset();

	FCamera& GetCamera() { return mCamera; }

	void SetViewportArea(float InLeft, float InTop, float InWidth, float InHeight);

	// FViewportClient 인터페이스 구현
	virtual void Draw(FViewport* Viewport, FGraphicsManager* GraphicsMgr, FSceneManager* SceneMgr) override;

	// 하위 호환성을 위한 오버로드
	void Draw(FGraphicsManager* GraphicsMgr, FSceneManager* SceneMgr);
	void DrawViewportUI(
		const FRect& rect,
		int32 ViewportIndex,
		FEditorViewportClient*& InOutActiveViewport,
		int32& InOutMaximizedIndex,
		AActor* SelectedActor,
		const ImVec2& startCursorPos,
		const ImVec2& screenCursorPos
	);

	// 상단 툴바 UI 렌더링 (FViewport::DrawViewportUI에서 위임)
	void DrawToolbar(
		FViewport* Viewport,
		const FRect& rect,
		int32 ViewportIndex,
		FEditorViewportClient*& InOutActiveViewport,
		int32& InOutMaximizedIndex,
		AActor* SelectedActor,
		const ImVec2& startCursorPos,
		const ImVec2& screenCursorPos
	);

	void SetViewportType(EViewportType InType);
	void FocusOnActor(AActor* TargetActor);

	float GetPerspectiveRatio(float GlobalPerspectiveRatio = 1.0f) const { return bIsOrthographic ? 0.0f : GlobalPerspectiveRatio; }

	virtual void SetViewport(FViewport* InViewport) override { mViewport = InViewport; }
	virtual FViewport* GetViewport() const override { return mViewport; }

	uint32 GetWidth() const;
	uint32 GetHeight() const;
	float GetViewportLeft() const;
	float GetViewportTop() const;

	void SaveConfig(const char* Section, const char* IniPath = ".\\editor.ini");
	void LoadConfig(const char* Section, const char* IniPath = ".\\editor.ini");

	EViewportType ViewportType = EViewportType::Perspective;
	EViewModeIndex ViewMode = EViewModeIndex::VMI_Lit;
	bool bIsOrthographic = false;

	FCamera mCamera;
	FGizmo mGizmo;
	FViewport* mViewport = nullptr;
	uint32 mWidth = 800;
	uint32 mHeight = 600;
	float mViewportX = 0.0f;
	float mViewportY = 0.0f;
	// 자신의 스플리터 영역 내 상대 위치
	float mViewportLeft = 0.0f;
	float mViewportTop = 0.0f;

	void FocusOnViewerActor();
	void SetViewerActor(AActor* InActor);
	void UpdateViewerCamera();
	AActor* mViewerActor = nullptr;

	FVector mViewerTarget = FVector(0.0f, 0.0f, 0.0f);
	float mViewerYaw = 0.0f;
	float mViewerPitch = 0.0f;
	float mViewerDistance = 5.0f;
	float mBaseRadius = 1.0f;
	float mZoomFactor = 3.0f;

	// 이 뷰포트에서 어떤 stat 커맨드가 켜져 있는가(UE5처럼 뷰포트마다 따로 켤 수 있다).
	// 콘솔의 "Stat X" 명령이 ActiveViewportClient의 이 맵을 바꾼다.
	TMap<FName, bool, FNameHasher> StatCommands;

	// 콘솔 "Stat X" 명령이 호출한다. 이 뷰포트의 표시 여부를 바꾸고, 모든 뷰포트를 OR로
	// 합쳐 FStatManager의 실측 여부(bEnabled)를 다시 계산한다.
	void ToggleStatCommand(const FName& CommandName);
	void ClearStatCommands();

	// 뷰포트마다 렌더타겟과 2D 투영이 따로이므로, 그 뷰포트를 Render()하기 직전에
	// (그 뷰포트 자신의 폭/높이와 자신의 StatCommands로) 호출해야 한다. Update()에서 부르면
	// ActiveViewportClient 기준으로 만들어진 쿼드가 렌더 루프의 4개 뷰포트 모두에 그대로 찍혀버린다.
	void DrawStatsHUD(const TMap<FName, bool, FNameHasher>& InStatCommands, UFontAtlas* Atlas,
		FRenderCollector& Collector, float ViewportW, float ViewportH);

private:
	// 모든 뷰포트의 StatCommands를 OR로 합쳐 FStatManager::RefreshEnabled를 호출한다.
	// 실측 코드는 뷰포트 구분 없이 전역으로 한 번만 돌기 때문에, 어느 한 뷰포트라도
	// 요구하면 그 스탯의 수집 자체는 켜져 있어야 한다.
	static void RefreshGlobalStatEnabled();

private:

	void GatherStatFPS(TArray<FStatRow>& Rows, bool bUnitAlsoEnabled);
	void GatherStatUnit(TArray<FStatRow>& Rows);

	void DrawStatMemoryTable(UFontAtlas* Atlas, FRenderCollector& RenderCollector, float ViewportW);

	// 단색 사각형. 표의 줄무늬 배경
	static void DrawStatRect(FRenderCollector& RenderCollector, float X, float Y, float W, float H, const FVector4& Color);

	void DrawStatRows(UFontAtlas* Atlas, FRenderCollector& RenderCollector, const TArray<FStatRow>& Rows, float ViewportW);
	static float MeasureStatText(FFontAtlas* FontAtlas, const char* Text, float Scale);
	static void DrawStatText(UFontAtlas* Atlas, FRenderCollector& RenderCollector,const char* Text, float LeftX, float Y, float Scale, const FVector4& Color);

	// HUD에 그릴 글자 크기(픽셀).
	static constexpr float StatFontPixelSize = 18.0f;

	// 화면 가장자리에서 띄우는 여백
	static constexpr float StatScreenMargin = 48.0f;

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
