#include "FEditorViewportClient.h"

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
#include "EngineMathLibrary.h"
#include "PrimitiveComponent.h"
#include "RayCast.h"

FEditorViewportClient::FEditorViewportClient(URenderer& InRenderer)
	: mCamera(FTransform({ -2.0f, 1.0f, 1.0f }, { 0, 30, 0 }, { 1, 1, 1 }))
	, mGizmo(InRenderer)
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
	const float ViewportWidth = SceneManager.GetViewportWidth();
	const float ViewportHeight = SceneManager.GetViewportHeight();
	if (ViewportWidth <= 0.f || ViewportHeight <= 0.f)
	{
		return nullptr;
	}

	const int32 MouseXInViewport = WindowApplication.Input.CursorX - static_cast<int32>(SceneManager.GetViewportX());
	const int32 MouseYInViewport = WindowApplication.Input.CursorY - static_cast<int32>(SceneManager.GetViewportY());

	// 투영 방식에 따라 광선을 만드는 법만 다르다. 두 점을 구하고 나면 이후 판정은 완전히 같다
	FVector NearPoint, FarPoint;
	DeprojectScreenToWorldForUnified(MouseXInViewport, MouseYInViewport,
		ViewportWidth, ViewportHeight, 0.1f, 100.f, mCamera.mOrthoDistance, perspectiveRatio, NearPoint, FarPoint);

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

	// Camera Rotate
	// 회전을 이동보다 먼저, 이번 프레임에 돌린 방향으로 바로 움직이게
	if (bAllowMouse && Input.IsDown(VK_RBUTTON))
	{
		mCamera.Rotate(Input.MouseDX, Input.MouseDY);
	}

	// Camera Velocity
	FVector MoveDir(0.f, 0.f, 0.f);
	if (bAllowKeyboardInput)
	{
		const FMatrix R = FMatrix::Rotate(mCamera.Transform.Rotation);
		const FVector Forward = R.GetUnitAxis(EAxis::X);
		const FVector Right = R.GetUnitAxis(EAxis::Y);

		if (Input.IsDown('W')) MoveDir += Forward;
		if (Input.IsDown('S')) MoveDir -= Forward;
		if (Input.IsDown('D')) MoveDir += Right;
		if (Input.IsDown('A')) MoveDir -= Right;
		if (Input.IsDown('E')) MoveDir += FVector(0.f, 0.f, 1.f);
		if (Input.IsDown('Q')) MoveDir -= FVector(0.f, 0.f, 1.f);
	}

	const bool bMoveKeyDown = !MoveDir.IsNearlyZero();
	if (bMoveKeyDown)
	{
		MoveDir.Normalize();
	}

	//Camera Translate
	if (bAllowMouse && Input.MouseWheelDelta != 0.0f)
	{
		//키 입력이 없으면 마우스 휠은 줌인/줌아웃
		if (!bMoveKeyDown)
		{
			if (perspectiveRatio < 1.0f)
			{
				mCamera.mOrthoDistance *= FMath::Pow(1.2f, -Input.MouseWheelDelta);
				mCamera.mOrthoDistance = FMath::Clamp(mCamera.mOrthoDistance, 0.1f, 100.0f);
			}
			else
			{
				mCamera.Transform.Location += mCamera.GetForwardVector() * 1.0f * Input.MouseWheelDelta;
			}
		}
		//입력이 있으면 마우스 휠은 카메라 이동속도 조절
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

void FEditorViewportClient::SetTargetSize(float InWidth, float InHeight)
{
	mWidth = std::max<uint32>(1, static_cast<uint32>(InWidth));
	mHeight = std::max<uint32>(1, static_cast<uint32>(InHeight));
}

void FEditorViewportClient::ResizeRenderTarget(FGraphicsManager* GraphicsManager)
{
	if (mRenderTarget == nullptr ||
		mRenderTarget->Width != mWidth ||
		mRenderTarget->Height != mHeight)
	{
		URenderer* Renderer = GraphicsManager->GetRenderer();

		mRenderTarget = Renderer->CreateRenderTarget2D(mWidth, mHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
		mDepthStencil = Renderer->CreateDepthStencil(mWidth, mHeight);
	}
}
