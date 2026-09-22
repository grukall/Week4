#include "FEditorViewportClient.h"
#include "FViewport.h"

#include "Cube.h"
#include "Sphere.h"
#include "Triangle.h"
#include "GizmoArrow.h"
#include "Circle.h"
#include "Plane.h"
#include "WindowApplication.h"
#include "ImGui/imgui.h"
#include "Console.h"
#include "SceneManager.h"
#include "MathUtility.h"
#include "GraphicsManager.h"
#include "Renderer.h"
#include <cstdio>
#include <cstdlib>
#include "EngineMathLibrary.h"
#include "PrimitiveComponent.h"
#include "RayCast.h"
#include "Assets.h"
#include "FFontAtlas.h"
#include "RenderInfo.h"
#include "GlobalFNames.h"
#include "LaunchEngineLoop.h"
#include "FAssetManager.h"
#include <psapi.h>
#include "Actor.h"
#include "UStaticMeshComponent.h"
FEditorViewportClient::FEditorViewportClient(URenderer& InRenderer)
	: mCamera(FTransform({ -2.0f, 1.0f, 1.0f }, { 0, 30, 0 }, { 1, 1, 1 }))
	, mGizmo(InRenderer)
	, mRenderer(&InRenderer)
{
	char Value[64] = {};
	GetPrivateProfileStringA("Camera", "Sensitivity", "", Value, sizeof(Value), ".\\editor.ini");
	float Sensitivity = 0.1f;
	if (sscanf_s(Value, "%f", &Sensitivity) == 1 && Sensitivity >= 0.01f && Sensitivity <= 1.0f)
	{
		mCamera.SetSensitivity(Sensitivity);
	}
}

AActor* FEditorViewportClient::PerformMousePicking(float perspectiveRatio, const FRenderCollector& RenderCollector, FSceneManager &SceneManager)
{
	bMouseHit = false;

	// 씬은 ImGui "Viewport" 창의 이미지 위에 그려진다.
	// 그래서 역투영에 넣을 좌표계 기준은 윈도우 전체가 아니라 그 이미지다.
	// 커서를 이미지 좌상단 기준으로 옮기고, 화면 크기도 이미지 크기를 쓴다.
	const float ViewportWidth = static_cast<float>(GetWidth());
	const float ViewportHeight = static_cast<float>(GetHeight());
	if (ViewportWidth <= 0.f || ViewportHeight <= 0.f)
	{
		return nullptr;
	}

	const float AbsoluteStartX = SceneManager.GetViewportX() + GetViewportLeft();
	const float AbsoluteStartY = SceneManager.GetViewportY() + GetViewportTop();

	const int32 MouseXInViewport = WindowApplication.Input.CursorX - static_cast<int32>(AbsoluteStartX);
	const int32 MouseYInViewport = WindowApplication.Input.CursorY - static_cast<int32>(AbsoluteStartY);

	const float EffectiveRatio = bIsOrthographic ? 0.0f : perspectiveRatio;

	// 투영 방식에 따라 광선을 만드는 법만 다르다. 두 점을 구하고 나면 이후 판정은 완전히 같다
	FVector NearPoint, FarPoint;
	DeprojectScreenToWorldForUnified(MouseXInViewport, MouseYInViewport,
		ViewportWidth, ViewportHeight, mCamera.nearZ, mCamera.farZ, mCamera.mOrthoDistance, EffectiveRatio, NearPoint, FarPoint);

	mRayNear = NearPoint;
	mRayFar = FarPoint;

	float NearlistT = FLT_MAX;
	AActor* NearestActor = nullptr;
	const FPickingRay PickingRay(NearPoint, FarPoint);

	// 충돌 판정은 컴포넌트가 스스로 한다. 여기서는 어느 것이 가장 가까운지만 고른다.
	for (UPrimitiveComponent* PickTarget : RenderCollector.PickTargets)
	{
		float HitT = FLT_MAX;
		if (!PickTarget->RayCastComponent(PickingRay, HitT))
		{
			continue;
		}

		if (HitT < NearlistT)
		{
			NearlistT = HitT;
			bMouseHit = true;
			NearestActor = PickTarget->GetOwner();  // 가장 가까운 액터를 반환
		}
	}

	return NearestActor;
}

void FEditorViewportClient::Update(float deltaTime, FSceneManager* sceneManager, float perspectiveRatio, FRenderCollector& RenderCollector)
{
	const FInputState& Input = WindowApplication.Input;
	bool bAllowMouse = sceneManager->IsViewportHovered();
	bool bAllowKeyboardInput = bAllowMouse && !ImGui::GetIO().WantCaptureKeyboard;
#if IS_OBJ_VIEWER

	if (bAllowMouse && Input.IsDown(VK_LBUTTON) && mViewerActor != nullptr) {
		constexpr float RotationSensitivity = 0.5f;
		mViewerYaw += Input.MouseDX * RotationSensitivity;
		mViewerPitch += Input.MouseDY * RotationSensitivity;
		mViewerPitch = FMath::Clamp(mViewerPitch, -89.0f, 89.0f);
		UpdateViewerCamera();
	}

	if (bAllowMouse && Input.MouseWheelDelta != 0.0f) {
		mViewerDistance *= FMath::Pow(1.2f, -Input.MouseWheelDelta);
		float minDistance = mBaseRadius * 0.1f;
		float maxDistance = mBaseRadius * 10.0f;
		mViewerDistance = FMath::Clamp(mViewerDistance, minDistance, maxDistance);

		if (mBaseRadius > KINDA_SMALL_NUMBER) {
			mZoomFactor = mViewerDistance / mBaseRadius;
		}

		UpdateViewerCamera();
	}

	mCamera.Velocity = FVector(0.0f);
#else
	// Camera Rotate
	// 회전을 이동보다 먼저, 이번 프레임에 돌린 방향으로 바로 움직이게
	if (bAllowMouse && Input.IsDown(VK_RBUTTON))
	{
		if (!bIsOrthographic)
		{
			mCamera.Rotate(Input.MouseDX, Input.MouseDY);
		}
		else
		{
			// 직교 뷰에서는 회전 대신 마우스 우클릭 드래그로 화면 패닝(이동)
			const FMatrix R = FMatrix::Rotate(mCamera.Transform.Rotation);
			const FVector Right = R.GetUnitAxis(EAxis::Y);
			const FVector Up = R.GetUnitAxis(EAxis::Z);
			const float PanSpeed = mCamera.mOrthoDistance * 0.003f;
			mCamera.Transform.Location -= Right * (static_cast<float>(Input.MouseDX) * PanSpeed);
			mCamera.Transform.Location += Up * (static_cast<float>(Input.MouseDY) * PanSpeed);
		}
	}

	// Camera Velocity
	FVector MoveDir(0.f, 0.f, 0.f);
	if (bAllowKeyboardInput)
	{
		const FMatrix R = FMatrix::Rotate(mCamera.Transform.Rotation);
		const FVector Forward = R.GetUnitAxis(EAxis::X);
		const FVector Right = R.GetUnitAxis(EAxis::Y);
		const FVector Up = R.GetUnitAxis(EAxis::Z);

		if (bIsOrthographic)
		{
			if (Input.IsDown('W')) MoveDir += Up;
			if (Input.IsDown('S')) MoveDir -= Up;
			if (Input.IsDown('D')) MoveDir += Right;
			if (Input.IsDown('A')) MoveDir -= Right;
		}
		else
		{
			if (Input.IsDown('W')) MoveDir += Forward;
			if (Input.IsDown('S')) MoveDir -= Forward;
			if (Input.IsDown('D')) MoveDir += Right;
			if (Input.IsDown('A')) MoveDir -= Right;
			if (Input.IsDown('E')) MoveDir += FVector(0.f, 0.f, 1.f);
			if (Input.IsDown('Q')) MoveDir -= FVector(0.f, 0.f, 1.f);
		}
	}

	const bool bMoveKeyDown = !MoveDir.IsNearlyZero();
	if (bMoveKeyDown)
	{
		MoveDir.Normalize();
	}

	// Camera Translate / Zoom
	if (bAllowMouse && Input.MouseWheelDelta != 0.0f)
	{
		// 키 입력이 없으면 마우스 휠은 줌인/줌아웃
		if (!bMoveKeyDown)
		{
			if (bIsOrthographic)
			{
				mCamera.mOrthoDistance *= FMath::Pow(1.2f, -Input.MouseWheelDelta);
				mCamera.mOrthoDistance = FMath::Clamp(mCamera.mOrthoDistance, 0.1f, 1000.0f);
			}
			else
			{
				mCamera.Transform.Location += mCamera.GetForwardVector() * 1.0f * Input.MouseWheelDelta;
			}
		}
		// 입력이 있으면 마우스 휠은 카메라 이동속도 조절
		else
		{
			mCamera.Speed *= FMath::Pow(1.2f, Input.MouseWheelDelta);
			mCamera.Speed = FMath::Clamp(mCamera.Speed, 0.1f, 100.0f);
		}
	}

	const FVector TargetVelocity = MoveDir * mCamera.Speed;

	// 지수 감쇠만큼 카메라 속도가 서서히 줄어듬
	const float Alpha = FMath::Exp(-mCamera.Damping * deltaTime);
	mCamera.Velocity = TargetVelocity + (mCamera.Velocity - TargetVelocity) * Alpha;
	if (mCamera.Velocity.IsNearlyZero())
	{
		mCamera.Velocity = FVector(0.f);
	}

	mCamera.Transform.Location += mCamera.Velocity * deltaTime;

	if (bAllowKeyboardInput)
	{
		if (Input.WasPressed('F'))
		{
			FocusOnActor(sceneManager->GetSelectedActor());
		}

		if (Input.WasPressed(VK_SPACE))
		{
			mGizmo.SetOperation(static_cast<EGIZMO_TYPE>((static_cast<int32>(mGizmo.GetOperation()) + 1) % 3));
		}

		if (Input.WasPressed('V'))
		{
			mGizmo.SetWorldMode(true);
		}
		else if (Input.WasPressed('B'))
		{
			mGizmo.SetWorldMode(false);
		}
	}

	// 스탯 HUD는 여기서 그리지 않는다. 뷰포트마다 렌더타겟과 2D 투영이 따로이므로,
	// 그 뷰포트를 Render()하기 직전에 그 뷰포트 자신의 폭/높이로 그려야 한다.
	// (렌더 루프에서 DrawStatsHUD를 직접 호출한다.)

#endif
}

namespace
{
	// 언리얼처럼 프레임 시간에 따라 색을 바꾼다. 60fps/30fps가 경계.
	FVector4 MsToColor(double Ms)
	{
		if (Ms > 33.3) return FVector4(1.0f, 0.35f, 0.35f, 1.0f);
		if (Ms > 16.6) return FVector4(1.0f, 0.85f, 0.35f, 1.0f);
		return FVector4(0.45f, 1.0f, 0.45f, 1.0f);
	}

	const FVector4 StatLabelColor(0.88f, 0.88f, 0.88f, 1.0f);
	const FVector4 StatValueColor(1.0f, 1.0f, 1.0f, 1.0f);

	// 라벨 열과 값 열 사이 간격(픽셀).
	constexpr float StatColumnGap = 10.0f;

	void AddStatRow(TArray<FStatRow>& Rows, const char* Label, const char* Value, const FVector4& Color)
	{
		FStatRow Row;
		if (Label)
			_snprintf_s(Row.Label, sizeof(Row.Label), _TRUNCATE, "%s:", Label);
		_snprintf_s(Row.Value, sizeof(Row.Value), _TRUNCATE, "%s", Value);
		Row.Color = Color;
		Rows.Add(Row);
	}

	// 언리얼의 Prims 표기처럼 큰 수는 K로 줄인다.
	void FormatCount(char* OutBuffer, size_t BufferSize, double Value)
	{
		if (Value >= 10000.0)
		{
			_snprintf_s(OutBuffer, BufferSize, _TRUNCATE, "%7.1fK", Value / 1000.0);
		}
		else
		{
			// double -> int32 캐스팅은 범위를 벗어나면 UB다. 여기 오는 값은 10000 미만이지만
			// 음수나 NaN이 섞여 들어와도 조용히 지나가도록 잘라둔다.
			const double Clamped = (Value > 0.0) ? FPlatformMath::Min(Value, 10000.0) : 0.0;
			_snprintf_s(OutBuffer, BufferSize, _TRUNCATE, "%8d", static_cast<int32>(Clamped));
		}
	}

	// 바이트를 KB/MB/GB 중 읽기 좋은 단위로.
	// bPad=true면 숫자 폭을 6칸으로 맞춘다. 값이 바뀌어도 열이 흔들리지 않게 하려는 것이라,
	// 문자열 중간에 들어가는 값(VRam의 Budget 등)에는 false로 사용.
	void FormatBytes(char* OutBuffer, size_t BufferSize, double Bytes, bool bPad = true)
	{
		constexpr double KB = 1024.0;
		constexpr double MB = KB * 1024.0;
		constexpr double GB = MB * 1024.0;

		const char* Unit = "KB";
		double Value = Bytes / KB;

		if (Bytes >= GB)      { Unit = "GB"; Value = Bytes / GB; }
		else if (Bytes >= MB) { Unit = "MB"; Value = Bytes / MB; }

		_snprintf_s(OutBuffer, BufferSize, _TRUNCATE,
			bPad ? "%6.2f %s" : "%.2f %s", Value, Unit);
	}
}

void FEditorViewportClient::FocusOnViewerActor()
{
	if (mViewerActor == nullptr) return;
	UStaticMeshComponent* MeshComponent = static_cast<UStaticMeshComponent*>(mViewerActor->GetRootComponent());
	if (MeshComponent == nullptr) return;
	UStaticMesh* StaticMesh = MeshComponent->GetStaticMesh();
	if (StaticMesh == nullptr) return;
	const FAABB& Bounds = StaticMesh->GetLocalBoundingBox();

	const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
	const FVector Extent = (Bounds.Max - Bounds.Min) * 0.5f;

	const FVector ComponentScale = MeshComponent->GetRelativeScale3D();

	const FVector ScaledExtent(Extent.x * FMath::Abs(ComponentScale.x), Extent.y * FMath::Abs(ComponentScale.y), Extent.z * FMath::Abs(ComponentScale.z));

	const float Radius = ScaledExtent.Length();

	if (Radius <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	mBaseRadius = Radius;
	mViewerTarget = Center;
	mViewerDistance = Radius * mZoomFactor;
	mCamera.farZ = mViewerDistance + Radius * 10.f;
	UpdateViewerCamera();
}

void FEditorViewportClient::SetViewerActor(AActor* InActor)
{
	mViewerActor = InActor;
}

void FEditorViewportClient::UpdateViewerCamera()
{
#if IS_OBJ_VIEWER
	constexpr float DegreeToRadian = PI / 180.0f;
	constexpr float RadianToDegree = 180.0f / PI;
	const float YawRadians = mViewerYaw * DegreeToRadian;
	const float PitchRadians = mViewerPitch * DegreeToRadian;
	const float CosPitch = cosf(PitchRadians);
	const float SinPitch = sinf(PitchRadians);
	const float CosYaw = cosf(YawRadians);
	const float SinYaw = sinf(YawRadians);

	const FVector Offset(CosPitch * CosYaw, CosPitch * SinYaw, SinPitch);

	FVector WorldTarget = mViewerTarget;
	if (mViewerActor != nullptr) {
		FVector ActorLocation = mViewerActor->GetTransform().Location;
		FRotator ActorRotation = mViewerActor->GetTransform().Rotation;
		FVector ActorScale = mViewerActor->GetTransform().Scale;

		FVector ScaledTarget = FVector(mViewerTarget.x * ActorScale.x, mViewerTarget.y * ActorScale.y, mViewerTarget.z * ActorScale.z);
		FMatrix RotMatrix = FMatrix::Rotate(ActorRotation);
		FVector RotatedTarget = RotMatrix.TransformPosition(ScaledTarget);

		WorldTarget = ActorLocation + RotatedTarget;
	}

	mCamera.Transform.Location = WorldTarget + Offset * mViewerDistance;

	FVector Direction = (WorldTarget - mCamera.Transform.Location);
	Direction.Normalize();

	const float Yaw = atan2f(Direction.y, Direction.x);
	const float HorizontalLength = sqrtf(Direction.x * Direction.x + Direction.y * Direction.y);
	const float Pitch = atan2f(Direction.z, HorizontalLength);
	mCamera.Transform.Rotation = FRotator(Pitch * RadianToDegree, Yaw * RadianToDegree, 0.0f);
#endif
}

namespace
{
	// TMap의 const operator[]는 std::map::at()과 같아서 키가 없으면 던진다.
	// 커맨드를 한 번도 안 건드린 뷰포트는 StatCommands가 비어있을 수 있으므로 Find로 안전하게 읽는다.
	bool IsStatCommandOn(const TMap<FName, bool, FNameHasher>& Commands, const FName& CommandName)
	{
		const bool* Value = Commands.Find(CommandName);
		return Value ? *Value : false;
	}
}

void FEditorViewportClient::DrawStatsHUD(const TMap<FName, bool, FNameHasher>& InStatCommands, UFontAtlas* Atlas, FRenderCollector& RenderCollector, float ViewportW, float ViewportH)
{
	// 열을 맞추려면 모든 줄의 라벨/값 폭을 알아야 하므로 먼저 다 모은다.
	TArray<FStatRow> Rows;

	const bool bUnit = IsStatCommandOn(InStatCommands, Name_UNIT);

	if (IsStatCommandOn(InStatCommands, Name_FPS))
	{
		GatherStatFPS(Rows, bUnit);
	}
	if (bUnit)
	{
		GatherStatUnit(Rows);
	}

	if (Rows.Num() > 0)
	{
		DrawStatRows(Atlas, RenderCollector, Rows, ViewportW);
	}

	// 메모리는 표 형태라 좌상단에 따로 그린다. 우상단 블록과 겹치지 않는다.
	if (IsStatCommandOn(InStatCommands, Name_MEMORY))
	{
		DrawStatMemoryTable(Atlas, RenderCollector, ViewportW);
	}
}

void FEditorViewportClient::ToggleStatCommand(const FName& CommandName)
{
	StatCommands[CommandName] = !StatCommands[CommandName];
	RefreshGlobalStatEnabled();
}

void FEditorViewportClient::ClearStatCommands()
{
	StatCommands[Name_UNIT] = false;
	StatCommands[Name_FPS] = false;
	StatCommands[Name_MEMORY] = false;
	RefreshGlobalStatEnabled();
}

void FEditorViewportClient::RefreshGlobalStatEnabled()
{
	bool bUnit = false;
	bool bFps = false;
	bool bMemory = false;

	for (FEditorViewportClient* Client : GEngineLoop.GetViewportClients())
	{
		if (!Client)
		{
			continue;
		}

		bUnit |= Client->StatCommands[Name_UNIT];
		bFps |= Client->StatCommands[Name_FPS];
		bMemory |= Client->StatCommands[Name_MEMORY];
	}

	FStatManager::Get().RefreshEnabled(bUnit, bFps, bMemory);
}

float FEditorViewportClient::MeasureStatText(FFontAtlas* FontAtlas, const char* Text, float Scale)
{
	float Width = 0.0f;
	for (const char* P = Text; *P; ++P)
	{
		const uint32 C = static_cast<uint8>(*P);

		if (!FontAtlas->HasGlyph(C))
		{
			FontAtlas->AddGlyph(C);
		}
		if (!FontAtlas->HasGlyph(C))
		{
			continue;
		}

		Width += FontAtlas->GetGlyph(C).AdvanceX * Scale;
	}
	return Width;
}

void FEditorViewportClient::DrawStatText(UFontAtlas* Atlas, FRenderCollector& RenderCollector,
	const char* Text, float LeftX, float Y, float Scale, const FVector4& Color)
{
	FFontAtlas* FontAtlas = Atlas->GetFontAtlas();

	// 베이스라인은 줄 상단에서 Ascender만큼 내려온 곳이다.
	const float BaselineY = Y + FontAtlas->Ascender() * Scale;
	float CursorX = LeftX;

	for (const char* P = Text; *P; ++P)
	{
		const uint32 C = static_cast<uint8>(*P);

		if (!FontAtlas->HasGlyph(C))
		{
			FontAtlas->AddGlyph(C);
		}
		if (!FontAtlas->HasGlyph(C))
		{
			continue;
		}

		const FFontGlyph& Glyph = FontAtlas->GetGlyph(C);

		const float W = Glyph.Width * Scale;
		const float H = Glyph.Height * Scale;
		const float X = CursorX + Glyph.BearingX * Scale;
		// BearingY는 베이스라인 위로 올라간 높이라 화면 Y(아래로 +)에서는 뺀다.
		const float GlyphY = BaselineY - Glyph.BearingY * Scale;

		// 공백처럼 비트맵이 없는 글자는 커서만 전진시킨다.
		if (W > 0.0f && H > 0.0f)
		{
			FRenderQuadInfo QuadInfo;
			// Quad2D의 로컬 사각형은 (0,0)~(1,1)이라 크기를 곱하고 좌상단으로 옮기면 끝이다.
			QuadInfo.Model = FMatrix::Scale(FVector3(W, H, 1.0f)) * FMatrix::Translation(FVector(X, GlyphY, 0.0f));
			QuadInfo.Color = Color;
			QuadInfo.TextureSRV = Atlas->GetSRV();
			QuadInfo.SubUV = Glyph.SubUV;
			QuadInfo.BlendMode = ERenderBlendMode::Transparent;
			QuadInfo.EnableDepthTest = false;
			QuadInfo.EnableDepthWrite = false;

			RenderCollector.AddQuadInfo(QuadInfo, true);
		}

		CursorX += Glyph.AdvanceX * Scale;
	}
}

void FEditorViewportClient::DrawStatRows(UFontAtlas* Atlas, FRenderCollector& RenderCollector,
	const TArray<FStatRow>& Rows, float ViewportW)
{
	FFontAtlas* FontAtlas = Atlas->GetFontAtlas();

	// 아틀라스를 구운 크기 기준의 수치를 원하는 크기로 환산하는 비율.
	const float Scale = StatFontPixelSize / FontAtlas->BakedPixelSize();
	const float LineHeight = FontAtlas->LineHeight() * Scale;

	// 1패스: 두 열의 최대 폭을 구한다. 여기서 없는 글리프도 다 채워진다.
	float MaxLabelWidth = 0.0f;
	float MaxValueWidth = 0.0f;
	for (uint32 i = 0; i < Rows.Num(); ++i)
	{
		MaxLabelWidth = FPlatformMath::Max(MaxLabelWidth, MeasureStatText(FontAtlas, Rows[i].Label, Scale));
		MaxValueWidth = FPlatformMath::Max(MaxValueWidth, MeasureStatText(FontAtlas, Rows[i].Value, Scale));
	}

	// 값 열은 왼쪽 정렬, 라벨 열은 오른쪽 정렬. 블록 전체를 화면 우상단에 붙인다.
	const float Margin = StatScreenMargin;
	const float ValueLeftX = ViewportW - Margin - MaxValueWidth;
	const float LabelRightX = ValueLeftX - StatColumnGap;

	float Y = Margin;
	for (uint32 i = 0; i < Rows.Num(); ++i)
	{
		const FStatRow& Row = Rows[i];

		const float LabelWidth = MeasureStatText(FontAtlas, Row.Label, Scale);
		DrawStatText(Atlas, RenderCollector, Row.Label, LabelRightX - LabelWidth, Y, Scale, StatLabelColor);
		DrawStatText(Atlas, RenderCollector, Row.Value, ValueLeftX, Y, Scale, Row.Color);

		Y += LineHeight;
	}
}

void FEditorViewportClient::GatherStatFPS(TArray<FStatRow>& Rows, bool bUnitAlsoEnabled)
{
	FStatManager& StatManager = FStatManager::Get();

	const double FrameMs = StatManager.GetDisplay(FName("Frame"));

	// 0으로 나누는 것만 막으면 부족하다. FrameMs가 극소값이면 Fps가 1e40 같은 값이 되고
	// "%6.2f"가 정수부만 수십 자리를 찍어 Buffer를 넘긴다. 실제로 있을 수 없는
	// 프레임 시간(1ns 미만)은 측정값이 아직 없는 것으로 보고 0 FPS로 표시한다.
	constexpr double MinFrameMs = 1e-6;
	const double Fps = FrameMs > MinFrameMs ? 1000.0 / FrameMs : 0.0;

	char Buffer[48];

	// sprintf_s는 버퍼가 모자라면 _invalid_parameter로 죽는다. HUD 문자열은
	// 잘려도 그만이므로 _TRUNCATE로 받는다.
	_snprintf_s(Buffer, sizeof(Buffer), _TRUNCATE, "%6.2f FPS", Fps);
	AddStatRow(Rows, nullptr, Buffer, MsToColor(FrameMs));

	// stat unit이 같이 켜져 있으면 Frame을 거기서 그리므로 중복해서 넣지 않는다.
	if (!bUnitAlsoEnabled)
	{
		_snprintf_s(Buffer, sizeof(Buffer), _TRUNCATE, "%6.2f ms", FrameMs);
		AddStatRow(Rows, nullptr, Buffer, MsToColor(FrameMs));
	}
}

void FEditorViewportClient::GatherStatUnit(TArray<FStatRow>& Rows)
{
	FStatManager& StatManager = FStatManager::Get();

	char Buffer[48];

	// TMap은 순서가 없으므로 등록 순서(Order)대로 정렬해서 모은다.
	TArray<const FStatEntry*> Sorted;
	TArray<FName> SortedNames;
	for (const auto& Pair : StatManager.Stats)
	{
		if (Pair.second.Type == EStatType::Memory || !Pair.second.bEnabled)
		{
			continue;
		}

		// 삽입 정렬. 항목이 십여 개라 이걸로 충분하다.
		uint32 Index = 0;
		while (Index < Sorted.Num() && Sorted[Index]->Order < Pair.second.Order)
		{
			++Index;
		}
		Sorted.Insert(&Pair.second, Index);
		SortedNames.Insert(Pair.first, Index);
	}

	// 1패스: 시간 항목
	for (uint32 i = 0; i < Sorted.Num(); ++i)
	{
		if (Sorted[i]->Type != EStatType::Cycle) continue;

		sprintf_s(Buffer, "%6.2f ms", Sorted[i]->Display);
		AddStatRow(Rows, SortedNames[i].ToString().CStr(), Buffer, MsToColor(Sorted[i]->Display));
	}

	// Mem: OS가 보는 프로세스 사용량이다.
	{
		PROCESS_MEMORY_COUNTERS_EX Counters = {};
		Counters.cb = sizeof(Counters);
		if (GetProcessMemoryInfo(GetCurrentProcess(),
			reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&Counters), sizeof(Counters)))
		{
			FormatBytes(Buffer, sizeof(Buffer), static_cast<double>(Counters.WorkingSetSize));
			AddStatRow(Rows, "Mem", Buffer, StatValueColor);
		}
	}

	// VRAM : GPU의 VRAM "사용량 / 예산(이 프로세스에게 OS가 허용한 양)"
	uint64 VramUsed = 0, VramBudget = 0;
	if (mRenderer->GetVideoMemoryInfo(VramUsed, VramBudget))
	{
		char UsedText[24], BudgetText[24];
		FormatBytes(UsedText, sizeof(UsedText), static_cast<double>(VramUsed));
		FormatBytes(BudgetText, sizeof(BudgetText), static_cast<double>(VramBudget), false);

		sprintf_s(Buffer, "%s / %s", UsedText, BudgetText);
		AddStatRow(Rows, "VRam", Buffer, StatValueColor);
	}

	// 2패스: 개수 항목
	for (uint32 i = 0; i < Sorted.Num(); ++i)
	{
		if (Sorted[i]->Type == EStatType::Cycle) continue;

		FormatCount(Buffer, sizeof(Buffer), Sorted[i]->Display);
		AddStatRow(Rows, SortedNames[i].ToString().CStr(), Buffer, StatValueColor);
	}
}

namespace
{
	// 언리얼 stat memory의 컬럼 구성.
	constexpr int32 StatMemColumnCount = 5;

	const char* StatMemHeaders[StatMemColumnCount] =
	{
		"Memory Counters", "UsedMax", "Mem%", "MemPool", "Pool Capacity"
	};

	// 첫 열(이름)만 왼쪽 정렬, 나머지 수치는 오른쪽 정렬.
	constexpr bool StatMemRightAlign[StatMemColumnCount] = { false, true, true, true, true };

	struct FStatMemRow
	{
		char Cells[StatMemColumnCount][48] = {};
	};

	const FVector4 StatTableTitleColor(1.00f, 0.55f, 0.15f, 1.0f);   // 주황: 제목/헤더
	const FVector4 StatTableTextColor(0.35f, 0.95f, 0.35f, 1.0f);    // 초록: 값
	const FVector4 StatTableRowColorA(0.10f, 0.10f, 0.10f, 0.72f);
	const FVector4 StatTableRowColorB(0.16f, 0.16f, 0.16f, 0.72f);

	constexpr float StatTableColumnGap = 34.0f;
	constexpr float StatTableCellPadding = 6.0f;
}

void FEditorViewportClient::DrawStatRect(FRenderCollector& RenderCollector,
	float X, float Y, float W, float H, const FVector4& Color)
{
	FRenderQuadInfo QuadInfo;
	QuadInfo.Model = FMatrix::Scale(FVector3(W, H, 1.0f)) * FMatrix::Translation(FVector(X, Y, 0.0f));
	QuadInfo.Color = Color;
	QuadInfo.TextureSRV = nullptr;   // 텍스처가 없으면 셰이더가 Color를 그대로 쓴다
	QuadInfo.BlendMode = ERenderBlendMode::Transparent;
	QuadInfo.EnableDepthTest = false;
	QuadInfo.EnableDepthWrite = false;

	RenderCollector.AddQuadInfo(QuadInfo, true);
}

void FEditorViewportClient::DrawStatMemoryTable(UFontAtlas* Atlas, FRenderCollector& RenderCollector, float ViewportW)
{
	FStatManager& StatManager = FStatManager::Get();
	FFontAtlas* FontAtlas = Atlas->GetFontAtlas();

	const float Scale = StatFontPixelSize / FontAtlas->BakedPixelSize();
	const float LineHeight = FontAtlas->LineHeight() * Scale;

	// 값이 큰 것부터. 언리얼도 UsedMax 내림차순으로 보여준다.
	TArray<FStatMemRow> Rows;
	TArray<double> SortKeys;

	double TotalBytes = 0.0;
	for (const auto& Pair : StatManager.Stats)
	{
		if (Pair.second.Type != EStatType::Memory)
		{
			continue;
		}

		TotalBytes += Pair.second.Display;

		FStatMemRow Row;
		_snprintf_s(Row.Cells[0], sizeof(Row.Cells[0]), _TRUNCATE, "%s", Pair.first.ToString().CStr());
		FormatBytes(Row.Cells[1], sizeof(Row.Cells[1]), Pair.second.Max);
		// 풀 개념이 없어서 Mem%와 Pool Capacity는 비운다. 언리얼도 풀이 아닌 항목은 비어 있다.
		_snprintf_s(Row.Cells[2], sizeof(Row.Cells[2]), _TRUNCATE, "%s", "");
		_snprintf_s(Row.Cells[3], sizeof(Row.Cells[3]), _TRUNCATE, "%s", "Physical");
		_snprintf_s(Row.Cells[4], sizeof(Row.Cells[4]), _TRUNCATE, "%s", "");

		// 삽입 정렬. 항목이 몇 개 안 된다.
		uint32 Index = 0;
		while (Index < SortKeys.Num() && SortKeys[Index] > Pair.second.Display)
		{
			++Index;
		}
		Rows.Insert(Row, Index);
		SortKeys.Insert(Pair.second.Display, Index);
	}

	if (Rows.Num() == 0)
	{
		return;
	}

	// 합계는 정렬에서 빼고 항상 맨 아래. 단위는 MB로 고정해 다른 줄과 비교하기 쉽게 둔다.
	{
		FStatMemRow TotalRow;
		_snprintf_s(TotalRow.Cells[0], sizeof(TotalRow.Cells[0]), _TRUNCATE, "%s", "Total");
		_snprintf_s(TotalRow.Cells[1], sizeof(TotalRow.Cells[1]), _TRUNCATE, "%6.2f MB", TotalBytes / (1024.0 * 1024.0));
		_snprintf_s(TotalRow.Cells[2], sizeof(TotalRow.Cells[2]), _TRUNCATE, "%s", "");
		_snprintf_s(TotalRow.Cells[3], sizeof(TotalRow.Cells[3]), _TRUNCATE, "%s", "Physical");
		_snprintf_s(TotalRow.Cells[4], sizeof(TotalRow.Cells[4]), _TRUNCATE, "%s", "");
		Rows.Add(TotalRow);
	}

	// 컬럼 폭은 헤더와 모든 셀 중 가장 넓은 것에 맞춘다.
	float ColumnWidth[StatMemColumnCount] = {};
	for (int32 Col = 0; Col < StatMemColumnCount; ++Col)
	{
		ColumnWidth[Col] = MeasureStatText(FontAtlas, StatMemHeaders[Col], Scale);
		for (uint32 i = 0; i < Rows.Num(); ++i)
		{
			ColumnWidth[Col] = FPlatformMath::Max(ColumnWidth[Col],
				MeasureStatText(FontAtlas, Rows[i].Cells[Col], Scale));
		}
	}

	float ColumnX[StatMemColumnCount] = {};
	const float Margin = StatScreenMargin;
	float Cursor = Margin + StatTableCellPadding;
	for (int32 Col = 0; Col < StatMemColumnCount; ++Col)
	{
		ColumnX[Col] = Cursor;
		Cursor += ColumnWidth[Col] + StatTableColumnGap;
	}
	const float TableWidth = Cursor - StatTableColumnGap + StatTableCellPadding - Margin;

	// 한 셀을 컬럼 정렬 규칙에 맞춰 그린다.
	auto DrawCell = [&](int32 Col, const char* Text, float Y, const FVector4& Color)
	{
		if (Text[0] == '\0')
		{
			return;
		}

		float X = ColumnX[Col];
		if (StatMemRightAlign[Col])
		{
			X += ColumnWidth[Col] - MeasureStatText(FontAtlas, Text, Scale);
		}
		DrawStatText(Atlas, RenderCollector, Text, X, Y, Scale, Color);
	};

	float Y = Margin;

	// 제목
	DrawStatText(Atlas, RenderCollector, "Memory [STATGROUP_MEMORY]",
		Margin + StatTableCellPadding, Y, Scale, StatTableTitleColor);
	Y += LineHeight;

	// 컬럼 헤더
	for (int32 Col = 0; Col < StatMemColumnCount; ++Col)
	{
		DrawCell(Col, StatMemHeaders[Col], Y, StatTableTitleColor);
	}
	Y += LineHeight;

	// 본문. 배경 줄무늬를 먼저 깔아야 글자가 위로 온다.
	for (uint32 i = 0; i < Rows.Num(); ++i)
	{
		DrawStatRect(RenderCollector, Margin, Y + i * LineHeight, TableWidth, LineHeight,
			(i % 2) == 0 ? StatTableRowColorA : StatTableRowColorB);
	}

	for (uint32 i = 0; i < Rows.Num(); ++i)
	{
		for (int32 Col = 0; Col < StatMemColumnCount; ++Col)
		{
			DrawCell(Col, Rows[i].Cells[Col], Y, StatTableTextColor);
		}
		Y += LineHeight;
	}
}

void FEditorViewportClient::DeprojectScreenToWorld(int32 MouseX, int32 MouseY, float ScreenW, float ScreenH, float NearZ, float FarZ, FVector& OutNearPoint, FVector& OutFarPoint)
{
	// 1) 픽셀 -> NDC. 화면 Y 는 아래로 +, NDC Y 는 위로 + 라서 뒤집는다
	const float ndcX = (2.0f * (MouseX + 0.5f) / ScreenW) - 1.0f;
	const float ndcY = 1.0f - (2.0f * (MouseY + 0.5f) / ScreenH);

	// 2) 투영 스케일 항 — GetProjectionMatrix 와 반드시 같은 식이어야 한다
	const float Aspect = ScreenW / ScreenH;
	const float yScale = 1.0f / tanf(mCamera.mFovDegree * 0.5f * PI / 180.f);
	const float xScale = yScale / Aspect;

	// 3) 카메라 기저로 월드 방향 합성. 전방 성분이 1 이므로 정규화하면 안 된다
	const FMatrix R = FMatrix::Rotate(mCamera.Transform.Rotation);
	FVector V = R.GetUnitAxis(EAxis::X);                    // 전방 (성분 1)
	V += R.GetUnitAxis(EAxis::Y) * (ndcX / xScale);         // 우측
	V += R.GetUnitAxis(EAxis::Z) * (ndcY / yScale);         // 상방

	// 4) 곱하면 그대로 각 평면 위의 점
	OutNearPoint = mCamera.Transform.Location + V * NearZ;
	OutFarPoint = mCamera.Transform.Location + V * FarZ;
}

void FEditorViewportClient::DeprojectScreenToWorldForOrtho(int32 MouseX, int32 MouseY, float ScreenW, float ScreenH, float NearZ, float FarZ, FVector& OutNearPoint, FVector& OutFarPoint)
{
	// 1) 픽셀 -> NDC. 화면 Y 는 아래로 +, NDC Y 는 위로 + 라서 뒤집는다
	const float ndcX = (2.0f * (MouseX + 0.5f) / ScreenW) - 1.0f;
	const float ndcY = 1.0f - (2.0f * (MouseY + 0.5f) / ScreenH);

	// 2) 화면이 담는 월드 크기 — GetOrthographicMatrix 에 넘기는 값과 반드시 같아야 한다.
	//    직교 행렬은 2/width, 2/height 로 나누므로 되돌리려면 절반을 곱한다
	const float Aspect = ScreenW / ScreenH;
	const float orthoHeight = mCamera.mOrthoHeight;
	const float orthoWidth = orthoHeight * Aspect;

	const FMatrix R = FMatrix::Rotate(mCamera.Transform.Rotation);
	const FVector Forward = R.GetUnitAxis(EAxis::X);
	const FVector Right = R.GetUnitAxis(EAxis::Y);
	const FVector Up = R.GetUnitAxis(EAxis::Z);

	// 3) 원근과 결정적으로 다른 점: 방향이 아니라 시작점이 픽셀마다 달라진다.
	//    모든 광선이 전방과 나란하고, 카메라 평면 위에서 평행이동한 자리에서 출발한다
	const FVector RayOrigin = mCamera.Transform.Location
		+ Right * (ndcX * orthoWidth * 0.5f)
		+ Up * (ndcY * orthoHeight * 0.5f);

	OutNearPoint = RayOrigin + Forward * NearZ;
	OutFarPoint = RayOrigin + Forward * FarZ;
}

void FEditorViewportClient::DeprojectScreenToWorldForUnified(
	int32 MouseX, int32 MouseY,
	float ScreenW, float ScreenH, float NearZ, float FarZ,
	float orthoDistance, float perspectiveRatio,
	FVector& OutNearPoint, FVector& OutFarPoint
)
{
	const float ndcX = (2.0f * (MouseX + 0.5f) / ScreenW) - 1.0f;
	const float ndcY = 1.0f - (2.0f * (MouseY + 0.5f) / ScreenH);

	const FMatrix invProjection = mCamera.GetInverseUnifiedProjectionMatrix(
		ScreenW / ScreenH, mCamera.mFovDegree, orthoDistance, NearZ, FarZ, perspectiveRatio
	);

	const FMatrix invViewProj = invProjection * mCamera.GetViewMatrix().AffineInverse();

	const auto Unproject = [&](float ndcZ) -> FVector
		{
			const FVector xyz = invViewProj.TransformPosition(FVector(ndcX, ndcY, ndcZ));

			const float w =
				ndcX * invViewProj.M[0][3] +
				ndcY * invViewProj.M[1][3] +
				ndcZ * invViewProj.M[2][3] +
				invViewProj.M[3][3];

			return xyz * (1.0f / w);
		};

	OutNearPoint = Unproject(0.0f);
	OutFarPoint = Unproject(1.0f);
}

void FEditorViewportClient::Reset()
{
	bMouseHit = false;
	mGizmo.Reset();
}

void FEditorViewportClient::SetViewportArea(float InLeft, float InTop, float InWidth, float InHeight)
{
	mViewportLeft = InLeft;
	mViewportTop = InTop;
	mWidth = std::max<uint32>(1, static_cast<uint32>(InWidth));
	mHeight = std::max<uint32>(1, static_cast<uint32>(InHeight));
}

uint32 FEditorViewportClient::GetWidth() const
{
	return mViewport ? mViewport->GetWidth() : mWidth;
}

uint32 FEditorViewportClient::GetHeight() const
{
	return mViewport ? mViewport->GetHeight() : mHeight;
}

float FEditorViewportClient::GetViewportLeft() const
{
	return mViewport ? mViewport->GetViewportLeft() : mViewportLeft;
}

float FEditorViewportClient::GetViewportTop() const
{
	return mViewport ? mViewport->GetViewportTop() : mViewportTop;
}

void FEditorViewportClient::SetViewportType(EViewportType InType)
{
	ViewportType = InType;

	switch (InType)
	{
	case EViewportType::Perspective:
		bIsOrthographic = false;
		break;
	case EViewportType::Top:
		bIsOrthographic = true;
		mCamera.Transform.Rotation = FRotator(-90.0f, 0.0f, 0.0f);
		mCamera.Transform.Location = FVector(0.0f, 0.0f, 20.0f);
		mCamera.Velocity = FVector(0.0f);
		break;
	case EViewportType::Bottom:
		bIsOrthographic = true;
		mCamera.Transform.Rotation = FRotator(90.0f, 0.0f, 0.0f);
		mCamera.Transform.Location = FVector(0.0f, 0.0f, -20.0f);
		mCamera.Velocity = FVector(0.0f);
		break;
	case EViewportType::Front:
		bIsOrthographic = true;
		mCamera.Transform.Rotation = FRotator(0.0f, 0.0f, 0.0f);
		mCamera.Transform.Location = FVector(-20.0f, 0.0f, 0.0f);
		mCamera.Velocity = FVector(0.0f);
		break;
	case EViewportType::Back:
		bIsOrthographic = true;
		mCamera.Transform.Rotation = FRotator(0.0f, 180.0f, 0.0f);
		mCamera.Transform.Location = FVector(20.0f, 0.0f, 0.0f);
		mCamera.Velocity = FVector(0.0f);
		break;
	case EViewportType::Left:
		bIsOrthographic = true;
		mCamera.Transform.Rotation = FRotator(0.0f, 90.0f, 0.0f);
		mCamera.Transform.Location = FVector(0.0f, -20.0f, 0.0f);
		mCamera.Velocity = FVector(0.0f);
		break;
	case EViewportType::Right:
		bIsOrthographic = true;
		mCamera.Transform.Rotation = FRotator(0.0f, -90.0f, 0.0f);
		mCamera.Transform.Location = FVector(0.0f, 20.0f, 0.0f);
		mCamera.Velocity = FVector(0.0f);
		break;
	}
}

void FEditorViewportClient::FocusOnActor(AActor* TargetActor)
{
	if (!TargetActor) return;

	const FVector TargetLoc = TargetActor->GetTransform().Location;

	if (ViewportType == EViewportType::Perspective)
	{
		const float FocusDist = 8.0f;
		mCamera.Transform.Location = TargetLoc - mCamera.GetForwardVector() * FocusDist;
		mCamera.LookAt(TargetLoc);
	}
	else
	{
		// 직교 뷰에서는 시선 방향 축은 유지하고 화면 중심을 타겟 위치로 맞춘다
		switch (ViewportType)
		{
		case EViewportType::Top:
		case EViewportType::Bottom:
			mCamera.Transform.Location.x = TargetLoc.x;
			mCamera.Transform.Location.y = TargetLoc.y;
			break;
		case EViewportType::Front:
		case EViewportType::Back:
			mCamera.Transform.Location.y = TargetLoc.y;
			mCamera.Transform.Location.z = TargetLoc.z;
			break;
		case EViewportType::Left:
		case EViewportType::Right:
			mCamera.Transform.Location.x = TargetLoc.x;
			mCamera.Transform.Location.z = TargetLoc.z;
			break;
		default:
			break;
		}
	}
	mCamera.Velocity = FVector(0.0f);
}

void FEditorViewportClient::Draw(FViewport* Viewport, FGraphicsManager* GraphicsMgr, FSceneManager* SceneMgr)
{
	if (!Viewport) return;

	// 렌더 타겟 바인딩 및 Clear
	GraphicsMgr->GetRenderer()->BindRenderTarget(Viewport->GetRenderTarget(), Viewport->GetDepthStencil(), true);

	const float currentWidth = static_cast<float>(Viewport->GetWidth());
	const float currentHeight = static_cast<float>(Viewport->GetHeight());
	const float CurrentRatio = GetPerspectiveRatio(GraphicsMgr->GetPerspectiveRatio());

	GraphicsMgr->Prepare(&mCamera, currentWidth, currentHeight, CurrentRatio, ViewMode);

	if (UFontAtlas* StatFontAtlas = GEngineLoop.GetAssetManager()->GetAssetAs<UFontAtlas>(FName("StatFontAtlas")))
	{
		DrawStatsHUD(StatCommands, StatFontAtlas, GraphicsMgr->GetRenderCollector(), currentWidth, currentHeight);
	}

	GraphicsMgr->FlushLines();
	GraphicsMgr->Render();
	GraphicsMgr->GetRenderCollector().ClearScreenQuads();

	if (SceneMgr && SceneMgr->GetSelectedActor())
	{
		FRenderInfo clickedRenderInfo;
		SceneMgr->GetSelectedActor()->GetFirstRenderInfo(clickedRenderInfo);
		GraphicsMgr->RenderHighLight(clickedRenderInfo);
	}

	// 각 뷰포트별 렌더링용 ViewProj 계산 및 기즈모 렌더링
	const float Aspect = Viewport->GetAspectRatio();
	const FMatrix CurrentViewProj = mCamera.GetViewMatrix() *
		mCamera.GetUnifiedProjectionMatrix(Aspect, mCamera.mFovDegree, mCamera.mOrthoDistance, 0.1f, 1000.f, CurrentRatio);

	mGizmo.Render(
		SceneMgr,
		mCamera.Transform.Location,
		CurrentViewProj,
		currentWidth,
		currentHeight
	);
}

void FEditorViewportClient::Draw(FGraphicsManager* GraphicsMgr, FSceneManager* SceneMgr)
{
	if (mViewport)
	{
		mViewport->Draw(GraphicsMgr, SceneMgr);
	}
}

void FEditorViewportClient::DrawViewportUI(
	const FRect& rect,
	int32 ViewportIndex,
	FEditorViewportClient*& InOutActiveViewport,
	int32& InOutMaximizedIndex,
	AActor* SelectedActor,
	const ImVec2& startCursorPos,
	const ImVec2& screenCursorPos
)
{
	if (mViewport)
	{
		mViewport->DrawViewportUI(rect, ViewportIndex, InOutActiveViewport, InOutMaximizedIndex, SelectedActor, startCursorPos, screenCursorPos);
	}
}

void FEditorViewportClient::DrawToolbar(
	FViewport* Viewport,
	const FRect& rect,
	int32 ViewportIndex,
	FEditorViewportClient*& InOutActiveViewport,
	int32& InOutMaximizedIndex,
	AActor* SelectedActor,
	const ImVec2& startCursorPos,
	const ImVec2& screenCursorPos
)
{
	const float ToolbarHeight = 26.0f;
	const float maxBtnWidth = 22.0f;
	const float rightButtonX = startCursorPos.x + rect.Right - maxBtnWidth - 4.0f;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 barMin(screenCursorPos.x + rect.Left, screenCursorPos.y + rect.Top);
	const ImVec2 barMax(screenCursorPos.x + rect.Right, screenCursorPos.y + rect.Top + ToolbarHeight);

	// 툴바 배경색 및 하단 구분선
	drawList->AddRectFilled(barMin, barMax, IM_COL32(28, 28, 32, 240));
	drawList->AddLine(ImVec2(barMin.x, barMax.y), ImVec2(barMax.x, barMax.y), IM_COL32(45, 45, 50, 255));

	ImGui::SetCursorPos(ImVec2(startCursorPos.x + rect.Left + 4.0f, startCursorPos.y + rect.Top + 2.0f));

	// 뷰포트 인덱스 배지
	bool bIsActive = (this == InOutActiveViewport);
	if (bIsActive)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f), "[#%d]", ViewportIndex + 1);
	}
	else
	{
		ImGui::TextDisabled("[#%d]", ViewportIndex + 1);
	}
	if (ImGui::IsItemClicked())
	{
		InOutActiveViewport = this;
	}

	ImGui::SameLine();

	// 시점 드롭다운 (View Type)
	const char* ViewTypeNames[] = { "Perspective", "Top", "Bottom", "Left", "Right", "Front", "Back" };
	int CurrentViewType = static_cast<int>(ViewportType);
	ImGui::SetNextItemWidth(110.0f);
	if (ImGui::Combo("##ViewType", &CurrentViewType, ViewTypeNames, IM_ARRAYSIZE(ViewTypeNames)))
	{
		SetViewportType(static_cast<EViewportType>(CurrentViewType));
		InOutActiveViewport = this;
	}

	ImGui::SameLine();

	// 뷰 모드 드롭다운 (View Mode)
	const char* ViewModes[] = { "Lit", "Unlit", "Wireframe" };
	int CurrentViewMode = static_cast<int>(ViewMode);
	ImGui::SetNextItemWidth(100.0f);
	if (ImGui::Combo("##ViewMode", &CurrentViewMode, ViewModes, IM_ARRAYSIZE(ViewModes)))
	{
		ViewMode = static_cast<EViewModeIndex>(CurrentViewMode);
		InOutActiveViewport = this;
	}

	ImGui::SameLine();

	// 카메라 속도 조절
	ImGui::SetNextItemWidth(48.0f);
	if (ImGui::DragFloat("##Speed", &mCamera.Speed, 0.2f, 0.1f, 50.0f, "S:%.1f"))
	{
		InOutActiveViewport = this;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Camera Speed: %.1f", mCamera.Speed);
	}

	// 너비 여유에 따라 FOV, Sens를 인라인 또는 팝업으로 제공
	const float spaceRemaining = rightButtonX - ImGui::GetCursorPosX();

	if (spaceRemaining >= 150.0f)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(50.0f);
		if (ImGui::DragFloat("##FOV", &mCamera.mFovDegree, 0.5f, 5.0f, 170.0f, "FOV:%.0f"))
		{
			InOutActiveViewport = this;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Field of View (FOV: %.1f deg)", mCamera.mFovDegree);
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(60.0f);
		if (ImGui::DragFloat("##Sens", &mCamera.Sensitivity, 0.005f, 0.01f, 1.0f, "Sens:%.2f", ImGuiSliderFlags_AlwaysClamp))
		{
			InOutActiveViewport = this;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Camera Sensitivity: %.3f", mCamera.Sensitivity);
		}
	}
	else
	{
		ImGui::SameLine();
		if (ImGui::Button("..."))
		{
			ImGui::OpenPopup("CamOptPopup");
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Camera Settings (FOV, Sensitivity)");
		}

		if (ImGui::BeginPopup("CamOptPopup"))
		{
			ImGui::Text("Camera Settings [#%d]", ViewportIndex + 1);
			ImGui::Separator();
			ImGui::SliderFloat("FOV", &mCamera.mFovDegree, 5.0f, 170.0f, "%.1f deg");
			ImGui::SliderFloat("Sensitivity", &mCamera.Sensitivity, 0.01f, 1.0f, "%.3f");
			ImGui::DragFloat("Speed", &mCamera.Speed, 0.2f, 0.1f, 50.0f, "%.1f");
			ImGui::EndPopup();
		}
	}

	ImGui::SameLine();

	// 포커스 버튼
	if (ImGui::Button("F"))
	{
		FocusOnActor(SelectedActor);
		InOutActiveViewport = this;
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Focus on selected actor (F)");
	}

	// 최대화 토글 버튼
	if (rightButtonX > ImGui::GetCursorPosX() + 4.0f)
	{
		ImGui::SetCursorPos(ImVec2(rightButtonX, startCursorPos.y + rect.Top + 2.0f));
		const char* maxIcon = (InOutMaximizedIndex == ViewportIndex) ? "■" : "□";
		if (ImGui::Button(maxIcon, ImVec2(maxBtnWidth, 0.0f)))
		{
			InOutMaximizedIndex = (InOutMaximizedIndex == ViewportIndex) ? -1 : ViewportIndex;
			InOutActiveViewport = this;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(InOutMaximizedIndex == ViewportIndex ? "Restore Viewport" : "Maximize Viewport");
		}
	}
}

