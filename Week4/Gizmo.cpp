#include "Gizmo.h"
#include "Actor.h"
#include "Renderer.h"
#include "ImGui/imgui.h"
#include "WindowApplication.h"
#include "EngineMathLibrary.h"
#include "SceneManager.h"
#include <cmath>

FGizmo::FGizmo(URenderer& InRenderer) 
	: Renderer(InRenderer) 
{
}

void FGizmo::Reset()
{
    bIsSelected = bIsHoveredAxis = false;
    SelectedAxis = HoveredAxis = EAxisNumber::None;
    TargetUUID = -1;
    HandleScreenSegments.Empty();
}

void FGizmo::BuildHandleSegments(const FVector& CameraLocation, const FMatrix& ViewProjection, float ViewportWidth, float ViewportHeight)
{

}

void FGizmo::SetWorldMode(bool bInWorldMode)
{
    bWorldMode = bInWorldMode;
}

void FGizmo::SetOperation(EGIZMO_TYPE Operation)
{
    CurrentOperation = Operation;
}

EGIZMO_TYPE FGizmo::GetOperation()
{ 
	return CurrentOperation; 
}

bool FGizmo::IsWorldMode()
{
    return bWorldMode;
}

bool FGizmo::IsMouseOverHandle() const 
{ 
	return bIsHoveredAxis; 
}

void FGizmo::Update(FSceneManager* SceneManager, const FMatrix& ViewProjection, float ViewportAbsX, float ViewportAbsY, float ViewportWidth, float ViewportHeight)
{
    AActor* TargetActor = SceneManager->GetSelectedActor();

    if (!TargetActor)
    {
        Reset();
        return;
    }

    if (TargetUUID != TargetActor->UUID)
    {
        Reset();
        TargetUUID = TargetActor->UUID;
    }

    const FInputState& Input = WindowApplication.Input;

    //HandleScreenSegments.Empty();
    //for (const auto& Axis : Gizmo3DAxes) // 3D 기즈모 축 정보
    //{
    //    // 월드 좌표 3D 축을 넘겨받은 Active 카메라의 ViewProj를 사용해 2D 스크린으로 투영
    //    FVector2 ScreenStart = WorldToScreen(Axis.WorldStart, ViewProjection, ViewportWidth, ViewportHeight);
    //    FVector2 ScreenEnd = WorldToScreen(Axis.WorldEnd, ViewProjection, ViewportWidth, ViewportHeight);

    //    HandleScreenSegments.Add({ ScreenStart, ScreenEnd, Axis.Direction, Axis.Type });
    //}

    FVector2 MousePosInScreen = Map(
        FVector2(Input.CursorX, Input.CursorY),
        FVector2(ViewportAbsX, ViewportAbsY),
        FVector2(ViewportAbsX + ViewportWidth, ViewportAbsY + ViewportHeight),
        FVector2(0.f, 0.f),
        FVector2(ViewportWidth, ViewportHeight)
    );

    const bool bAllowMouse = SceneManager->IsViewportHovered();
    bool bDragStarted = false;

    if (!Input.IsDown(VK_LBUTTON))
    {
        bIsSelected = false; SelectedAxis = EAxisNumber::None;
    }

    bIsHoveredAxis = false;
    HoveredAxis = EAxisNumber::None;

    if (bAllowMouse)
    {
        for (const FHandleSegment& Segment : HandleScreenSegments)
        {
            if (PointToLineSegmentDistanceSquared(MousePosInScreen, Segment.Start, Segment.End) >= HandleHitRadius * HandleHitRadius)
            {
                continue;
            }

            bIsHoveredAxis = true;
            HoveredAxis = Segment.Axis;
            if (!bIsSelected && Input.WasPressed(VK_LBUTTON))
            {
                PrevMousePos = MousePosInScreen;
                AxisDirection = Segment.Direction;
                HandleScreenStart = Segment.Start;
                HandleScreenDirection = Segment.End - Segment.Start;
                HandleScreenDirection.Normalize();
                DragStartLocation = TargetActor->GetTransform().Location;
                DragStartMousePosition = MousePosInScreen;
                bDragStarted = true;
                bIsSelected = true;
                SelectedAxis = Segment.Axis;
            }
            break;
        }
    }

    if (!bIsSelected)
    {
        return;
    }

    const float Sensitivity = 0.01f;
    const float Amount = FVector2::Dot(MousePosInScreen - PrevMousePos, HandleScreenDirection);

    const FTransform Transform = TargetActor->GetTransform();
    if (CurrentOperation == EGIZMO_TYPE::TRANSLATE)
    {
        float ProjectionLength = FVector2::Dot(MousePosInScreen - HandleScreenStart, HandleScreenDirection);
        FVector2 ProjectedPoint = HandleScreenStart + HandleScreenDirection * ProjectionLength;

        // Ray
        FMatrix ViewProjectionInverse = ViewProjection.Inverse();
        FVector NearPoint = ScreenToWorld(ProjectedPoint, ViewProjectionInverse, ViewportWidth, ViewportHeight, 0.1f);
        FVector FarPoint = ScreenToWorld(ProjectedPoint, ViewProjectionInverse, ViewportWidth, ViewportHeight, 1.0f);

        FRay Ray;
        Ray.Origin = NearPoint;
        Ray.Direction = FarPoint - NearPoint;
        Ray.Direction.Normalize();

        FVector W = DragStartLocation - Ray.Origin;

        float A = FVector::dot(AxisDirection, AxisDirection);
        float B = FVector::dot(AxisDirection, Ray.Direction);
        float C = FVector::dot(Ray.Direction, Ray.Direction);
        float D = FVector::dot(AxisDirection, W);
        float E = FVector::dot(Ray.Direction, W);

        float Denominator = A * C - B * B;
        if (FMath::Abs(Denominator) <= KINDA_SMALL_NUMBER)
        {
            return;
        }
        float T = (B * E - C * D) / Denominator;

        if (bDragStarted)
        {
            DragStartAxisParameter = T;
        }
        else
        {
            FVector NewLocation = DragStartLocation + AxisDirection * (T - DragStartAxisParameter);
            TargetActor->SetLocation(NewLocation);
        }
    }
    else if (CurrentOperation == EGIZMO_TYPE::ROTATE)
    {
        FQuaternion RotationQ = ToQuaternion(FMatrix::Rotate(Transform.Rotation));
        FQuaternion DeltaQ(AxisDirection, Amount * Sensitivity);
        FQuaternion FinalQ = DeltaQ * RotationQ;
        FinalQ.Normalize();
        const FVector Euler = ToEulerAngles(FinalQ) * (180.f / PI);
        TargetActor->SetRotation(FRotator(Euler.y, Euler.z, Euler.x));
    }
    else if (CurrentOperation == EGIZMO_TYPE::SCALE)
    {
        FVector DeltaScale(0.0f, 0.0f, 0.0f);
        if (!bWorldMode)
        {
            if (SelectedAxis == EAxisNumber::X) DeltaScale.x = Amount * Sensitivity;
            else if (SelectedAxis == EAxisNumber::Y) DeltaScale.y = Amount * Sensitivity;
            else if (SelectedAxis == EAxisNumber::Z) DeltaScale.z = Amount * Sensitivity;
        }
        else
        {
            const FMatrix Rotation = FMatrix::Rotate(Transform.Rotation);
            const FVector Lx = Rotation.GetUnitAxis(EAxis::X);
            const FVector Ly = Rotation.GetUnitAxis(EAxis::Y);
            const FVector Lz = Rotation.GetUnitAxis(EAxis::Z);

            if (SelectedAxis == EAxisNumber::X)
            {
                DeltaScale = FVector(fabsf(Lx.x), fabsf(Ly.x), fabsf(Lz.x)) * Amount * Sensitivity;
            }
            else if (SelectedAxis == EAxisNumber::Y)
            {
                DeltaScale = FVector(fabsf(Lx.y), fabsf(Ly.y), fabsf(Lz.y)) * Amount * Sensitivity;
            }
            else if (SelectedAxis == EAxisNumber::Z)
            {
                DeltaScale = FVector(fabsf(Lx.z), fabsf(Ly.z), fabsf(Lz.z)) * Amount * Sensitivity;
            }
        }

        FVector Scale = Transform.Scale + DeltaScale;
        Scale.x = FMath::Max(Scale.x, MIN_SCALE);
        Scale.y = FMath::Max(Scale.y, MIN_SCALE);
        Scale.z = FMath::Max(Scale.z, MIN_SCALE);

        TargetActor->SetScale(Scale);
    }

    PrevMousePos = MousePosInScreen;
}

void FGizmo::Render(FSceneManager* SceneManager, const FVector& CameraPosition, const FMatrix& ViewProjection, float ViewportWidth, float ViewportHeight)
{
    AActor* TargetActor = SceneManager->GetSelectedActor();

    HandleScreenSegments.Empty();

    if (!TargetActor) 
	{ 
		Reset(); 
		return; 
	}

    if (TargetUUID != TargetActor->UUID) 
	{ 
		Reset(); 
		TargetUUID = TargetActor->UUID; 
	}

    const FTransform Transform = TargetActor->GetTransform();
    const FVector CenterToCamera = CameraPosition - Transform.Location;
    const float AxisLength = 0.1f * CenterToCamera.Length();
    const float ScreenWidth = ViewportWidth;    // 기존: Renderer.GetWidth()
    const float ScreenHeight = ViewportHeight;   // 기존: Renderer.GetHeight()
    const FVector4 Clip = FVector4(Transform.Location, 1.f) * ViewProjection;
    const bool bDrawGizmo = !(Clip.w <= 0.00001f || Clip.z < 0.f || Clip.z > Clip.w || Clip.x < -Clip.w || Clip.x > Clip.w || Clip.y < -Clip.w || Clip.y > Clip.w);
	
	if (!bDrawGizmo)
	{
		return;
	}

    enum class EAxisEndPointStyle { 
        None, 
        Arrow, 
        Circle 
    };

    const FVector2 Center = WorldToScreen(Transform.Location, ViewProjection, ScreenWidth, ScreenHeight);
    const float ScaleX = static_cast<float>(Renderer.GetWidth()) / static_cast<float>(ScreenWidth);
    const float ScaleY = static_cast<float>(Renderer.GetHeight()) / static_cast<float>(ScreenHeight);
    auto ToRenderer = [&](const FVector2& P) { return FVector2(P.X * ScaleX, P.Y * ScaleY); };
    auto AxisColor = [&](EAxisNumber Axis, const FVector4& Color)
    {
        return Axis == (bIsSelected ? SelectedAxis : HoveredAxis) ? FVector4(1,1,0,1) : Color;
    };

    TArray<FRenderLineInfo> GizmoLines;

    auto DrawLineAxis = [&](const FVector& DrawAxis, const FVector& ApplyAxis, const FVector4& Color, EAxisEndPointStyle Style, EAxisNumber Axis)
    {
        const FVector EndWorld = Transform.Location + DrawAxis * AxisLength;
        const FVector2 End = WorldToScreen(EndWorld, ViewProjection, ScreenWidth, ScreenHeight);

        FVector2 ScreenAxis = End - Center;
		if (ScreenAxis.LengthSquared() < 0.01f)
		{
			return;
		}

        HandleScreenSegments.Add({ Center, End, ApplyAxis, Axis });
        
        const FVector2 RCenter = ToRenderer(Center);
        const FVector2 REnd = ToRenderer(End);
        const FVector2 RAxis = REnd - RCenter;

		const FVector4 Highlight = AxisColor(Axis, Color);
        GizmoLines.Add({ Highlight, Transform.Location, 5.f, EndWorld, 0.f });
        
		if (Style == EAxisEndPointStyle::Arrow)
		{
            Renderer.RenderTriangle2D(REnd, Highlight, 20.f, atan2f(RAxis.Y, RAxis.X));
		}
		else if (Style == EAxisEndPointStyle::Circle)
		{
            Renderer.RenderCircle2D(REnd, Highlight, 8.f);
		}
    };

    auto DrawCircleAxis = [&](const FVector& U, const FVector& V, const FVector4& Color, bool bNoClipping, EAxisNumber Axis)
    {
        constexpr int32 NumSegments = 32;
        FVector Points[NumSegments];
        GenerateCircleVertices([&](int32 Index, const FVector2& Point)
        {
            Points[Index] = Transform.Location + U * Point.X + V * Point.Y;
        }, AxisLength, NumSegments);

        for (int32 I = 0; I < NumSegments; ++I)
        {
            const FVector& StartWorld = Points[I];
            const FVector& EndWorld = Points[(I + 1) % NumSegments];
			if (!bNoClipping && FVector::dot(Lerp(StartWorld, EndWorld, 0.5f) - Transform.Location, CenterToCamera) < 0.f)
			{
				continue;
			}
            const FVector2 Start = WorldToScreen(StartWorld, ViewProjection, ScreenWidth, ScreenHeight);
            const FVector2 End = WorldToScreen(EndWorld, ViewProjection, ScreenWidth, ScreenHeight);
            
			if (FVector2::LengthSquared(Start, End) < 0.01f) 
			{
				continue;
			}

            HandleScreenSegments.Add({ Start, End, FVector::cross(U,V), Axis });
            GizmoLines.Add({ AxisColor(Axis, Color), StartWorld, 2.f, EndWorld, 0.f });
        }
    };

    const FMatrix Rotation = FMatrix::Rotate(Transform.Rotation);
    const bool bLocal = !bWorldMode;
    const FVector ForwardAxis = bLocal ? Rotation.GetUnitAxis(EAxis::X) : Front;
    const FVector RightAxis = bLocal ? Rotation.GetUnitAxis(EAxis::Y) : Right;
    const FVector UpAxis = bLocal ? Rotation.GetUnitAxis(EAxis::Z) : Up;

    if (CurrentOperation == EGIZMO_TYPE::TRANSLATE)
    {
        DrawLineAxis(ForwardAxis, ForwardAxis, FVector4(1, 0, 0, 1), EAxisEndPointStyle::Arrow, EAxisNumber::X);
        DrawLineAxis(RightAxis, RightAxis, FVector4(0, 1, 0, 1), EAxisEndPointStyle::Arrow, EAxisNumber::Y);
        DrawLineAxis(UpAxis, UpAxis, FVector4(0, 0, 1, 1), EAxisEndPointStyle::Arrow, EAxisNumber::Z);
    }
    else if (CurrentOperation == EGIZMO_TYPE::ROTATE)
    {
        DrawCircleAxis(RightAxis, UpAxis, FVector4(1, 0, 0, 1), false, EAxisNumber::X);
        DrawCircleAxis(UpAxis, ForwardAxis, FVector4(0, 1, 0, 1), false, EAxisNumber::Y);
        DrawCircleAxis(ForwardAxis, RightAxis, FVector4(0, 0, 1, 1), false, EAxisNumber::Z);

        FVector CameraAxisU = FVector::cross(CenterToCamera, Up);
        if (CameraAxisU.IsNearlyZero())
        {
            CameraAxisU = FVector::cross(CenterToCamera, Right);
        }

        if (!CameraAxisU.IsNearlyZero())
        {
            CameraAxisU.Normalize();

            FVector CameraAxisV = FVector::cross(CameraAxisU, CenterToCamera);
            CameraAxisV.Normalize();

            DrawCircleAxis(CameraAxisU, CameraAxisV, FVector4(1, 1, 1, 1), true, EAxisNumber::Cameara);
        }
    }
    else if (CurrentOperation == EGIZMO_TYPE::SCALE)
    {
        DrawLineAxis(ForwardAxis, ForwardAxis, FVector4(1, 0, 0, 1), EAxisEndPointStyle::Circle, EAxisNumber::X);
        DrawLineAxis(RightAxis, RightAxis, FVector4(0, 1, 0, 1), EAxisEndPointStyle::Circle, EAxisNumber::Y);
        DrawLineAxis(UpAxis, UpAxis, FVector4(0, 0, 1, 1), EAxisEndPointStyle::Circle, EAxisNumber::Z);
    }

    if (!GizmoLines.IsEmpty())
    {
        Renderer.RenderLines(GizmoLines, ViewProjection, FVector2(ScreenWidth, ScreenHeight), false);
    }

    Renderer.RenderCircle2D(ToRenderer(Center), FVector4(0.8f, 0.8f, 0.8f, 1), 5.f);
}
