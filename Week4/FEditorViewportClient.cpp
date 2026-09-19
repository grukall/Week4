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
#include "Assets.h"
#include "FFontAtlas.h"
#include "RenderInfo.h"
#include "GlobalFNames.h"
#include "LaunchEngineLoop.h"
#include "FAssetManager.h"

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

	//Stat정보 표시가 켜져 있으면 드로우한다.
	// 씬 렌더타겟은 백버퍼와 같은 크기이고 Projection2D도 그 크기로 만들어져 있다.
	// 그래서 렌더러 크기를 그대로 화면 좌표계로 쓴다.
	if (UFontAtlas* StatFontAtlas = GEngineLoop.GetAssetManager()->GetAssetAs<UFontAtlas>(FName("StatFontAtlas")))
	{
		DrawStatsHUD(FStatManager::Get(), StatFontAtlas, RenderCollector,
			static_cast<float>(mRenderer->GetWidth()), static_cast<float>(mRenderer->GetHeight()));
	}
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
		sprintf_s(Row.Label, "%s:", Label);
		sprintf_s(Row.Value, "%s", Value);
		Row.Color = Color;
		Rows.Add(Row);
	}

	// 언리얼의 Prims 표기처럼 큰 수는 K로 줄인다.
	void FormatCount(char* OutBuffer, size_t BufferSize, double Value)
	{
		if (Value >= 10000.0)
		{
			sprintf_s(OutBuffer, BufferSize, "%7.1fK", Value / 1000.0);
		}
		else
		{
			sprintf_s(OutBuffer, BufferSize, "%8d", static_cast<int32>(Value));
		}
	}

	// 바이트를 KB/MB/GB 중 읽기 좋은 단위로.
	void FormatBytes(char* OutBuffer, size_t BufferSize, double Bytes)
	{
		constexpr double KB = 1024.0;
		constexpr double MB = KB * 1024.0;
		constexpr double GB = MB * 1024.0;

		if (Bytes >= GB)      sprintf_s(OutBuffer, BufferSize, "%6.2f GB", Bytes / GB);
		else if (Bytes >= MB) sprintf_s(OutBuffer, BufferSize, "%6.2f MB", Bytes / MB);
		else                  sprintf_s(OutBuffer, BufferSize, "%6.2f KB", Bytes / KB);
	}
}

void FEditorViewportClient::DrawStatsHUD(FStatManager& StatManager, UFontAtlas* Atlas, FRenderCollector& RenderCollector, float ViewportW, float ViewportH)
{
	// 열을 맞추려면 모든 줄의 라벨/값 폭을 알아야 하므로 먼저 다 모은다.
	TArray<FStatRow> Rows;

	if (StatManager.StatCommands[Name_FPS])
	{
		GatherStatFPS(Rows);
	}
	if (StatManager.StatCommands[Name_UNIT])
	{
		GatherStatUnit(Rows);
	}

	if (Rows.Num() > 0)
	{
		DrawStatRows(Atlas, RenderCollector, Rows, ViewportW);
	}

	// 메모리는 표 형태라 좌상단에 따로 그린다. 우상단 블록과 겹치지 않는다.
	if (StatManager.StatCommands[Name_MEMORY])
	{
		DrawStatMemoryTable(Atlas, RenderCollector, ViewportW);
	}
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

		// 재지 않고 바로 그리는 문자열도 있으므로 여기서도 글리프를 채운다.
		// 이게 없으면 한 번도 측정된 적 없는 문자열은 글자가 통째로 빠진다.
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

void FEditorViewportClient::GatherStatFPS(TArray<FStatRow>& Rows)
{
	FStatManager& StatManager = FStatManager::Get();

	const double FrameMs = StatManager.GetDisplay(FName("Frame"));
	const double Fps = FrameMs > 0.0 ? 1000.0 / FrameMs : 0.0;

	char Buffer[48];

	sprintf_s(Buffer, "%6.2f FPS", Fps);
	AddStatRow(Rows, "FPS", Buffer, MsToColor(FrameMs));

	// stat unit이 같이 켜져 있으면 Frame을 거기서 그리므로 중복해서 넣지 않는다.
	if (!StatManager.StatCommands[Name_UNIT])
	{
		sprintf_s(Buffer, "%6.2f ms", FrameMs);
		AddStatRow(Rows, "Frame", Buffer, MsToColor(FrameMs));
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

	for (uint32 i = 0; i < Sorted.Num(); ++i)
	{
		const FStatEntry& Entry = *Sorted[i];

		if (Entry.Type == EStatType::Cycle)
		{
			sprintf_s(Buffer, "%6.2f ms", Entry.Display);
			AddStatRow(Rows, SortedNames[i].ToString().CStr(), Buffer, MsToColor(Entry.Display));
		}
		else
		{
			FormatCount(Buffer, sizeof(Buffer), Entry.Display);
			AddStatRow(Rows, SortedNames[i].ToString().CStr(), Buffer, StatValueColor);
		}
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
		sprintf_s(Row.Cells[0], "%s", Pair.first.ToString().CStr());
		FormatBytes(Row.Cells[1], sizeof(Row.Cells[1]), Pair.second.Display);
		// 풀 개념이 없어서 Mem%와 Pool Capacity는 비운다. 언리얼도 풀이 아닌 항목은 비어 있다.
		sprintf_s(Row.Cells[2], "%s", "");
		sprintf_s(Row.Cells[3], "%s", "Physical");
		sprintf_s(Row.Cells[4], "%s", "");

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
		sprintf_s(TotalRow.Cells[0], "%s", "Total");
		sprintf_s(TotalRow.Cells[1], "%6.2f MB", TotalBytes / (1024.0 * 1024.0));
		sprintf_s(TotalRow.Cells[2], "%s", "");
		sprintf_s(TotalRow.Cells[3], "%s", "Physical");
		sprintf_s(TotalRow.Cells[4], "%s", "");
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

	//Stat정보 표시가 켜져 있으면 드로우한다.
	// 씬 렌더타겟은 백버퍼와 같은 크기이고 Projection2D도 그 크기로 만들어져 있다.
	// 그래서 렌더러 크기를 그대로 화면 좌표계로 쓴다.
	if (UFontAtlas* StatFontAtlas = GEngineLoop.GetAssetManager()->GetAssetAs<UFontAtlas>(FName("StatFontAtlas")))
	{
		DrawStatsHUD(FStatManager::Get(), StatFontAtlas, RenderCollector,
			static_cast<float>(mRenderer->GetWidth()), static_cast<float>(mRenderer->GetHeight()));
	}
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
			sprintf_s(Row.Label, "%s:", Label);
		sprintf_s(Row.Value, "%s", Value);
		Row.Color = Color;
		Rows.Add(Row);
	}

	// 언리얼의 Prims 표기처럼 큰 수는 K로 줄인다.
	void FormatCount(char* OutBuffer, size_t BufferSize, double Value)
	{
		if (Value >= 10000.0)
		{
			sprintf_s(OutBuffer, BufferSize, "%7.1fK", Value / 1000.0);
		}
		else
		{
			sprintf_s(OutBuffer, BufferSize, "%8d", static_cast<int32>(Value));
		}
	}

	// 바이트를 KB/MB/GB 중 읽기 좋은 단위로.
	void FormatBytes(char* OutBuffer, size_t BufferSize, double Bytes)
	{
		constexpr double KB = 1024.0;
		constexpr double MB = KB * 1024.0;
		constexpr double GB = MB * 1024.0;

		if (Bytes >= GB)      sprintf_s(OutBuffer, BufferSize, "%6.2f GB", Bytes / GB);
		else if (Bytes >= MB) sprintf_s(OutBuffer, BufferSize, "%6.2f MB", Bytes / MB);
		else                  sprintf_s(OutBuffer, BufferSize, "%6.2f KB", Bytes / KB);
	}
}

void FEditorViewportClient::DrawStatsHUD(FStatManager& StatManager, UFontAtlas* Atlas, FRenderCollector& RenderCollector, float ViewportW, float ViewportH)
{
	// 열을 맞추려면 모든 줄의 라벨/값 폭을 알아야 하므로 먼저 다 모은다.
	TArray<FStatRow> Rows;

	if (StatManager.StatCommands[Name_FPS])
	{
		GatherStatFPS(Rows);
	}
	if (StatManager.StatCommands[Name_UNIT])
	{
		GatherStatUnit(Rows);
	}

	if (Rows.Num() > 0)
	{
		DrawStatRows(Atlas, RenderCollector, Rows, ViewportW);
	}

	// 메모리는 표 형태라 좌상단에 따로 그린다. 우상단 블록과 겹치지 않는다.
	if (StatManager.StatCommands[Name_MEMORY])
	{
		DrawStatMemoryTable(Atlas, RenderCollector, ViewportW);
	}
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

void FEditorViewportClient::GatherStatFPS(TArray<FStatRow>& Rows)
{
	FStatManager& StatManager = FStatManager::Get();

	const double FrameMs = StatManager.GetDisplay(FName("Frame"));
	const double Fps = FrameMs > 0.0 ? 1000.0 / FrameMs : 0.0;

	char Buffer[48];

	sprintf_s(Buffer, "%6.2f FPS", Fps);
	AddStatRow(Rows, nullptr, Buffer, MsToColor(FrameMs));

	// stat unit이 같이 켜져 있으면 Frame을 거기서 그리므로 중복해서 넣지 않는다.
	if (!StatManager.StatCommands[Name_UNIT])
	{
		sprintf_s(Buffer, "%6.2f ms", FrameMs);
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

	for (uint32 i = 0; i < Sorted.Num(); ++i)
	{
		const FStatEntry& Entry = *Sorted[i];

		if (Entry.Type == EStatType::Cycle)
		{
			sprintf_s(Buffer, "%6.2f ms", Entry.Display);
			AddStatRow(Rows, SortedNames[i].ToString().CStr(), Buffer, MsToColor(Entry.Display));
		}
		else
		{
			FormatCount(Buffer, sizeof(Buffer), Entry.Display);
			AddStatRow(Rows, SortedNames[i].ToString().CStr(), Buffer, StatValueColor);
		}
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
		sprintf_s(Row.Cells[0], "%s", Pair.first.ToString().CStr());
		FormatBytes(Row.Cells[1], sizeof(Row.Cells[1]), Pair.second.Max);
		// 풀 개념이 없어서 Mem%와 Pool Capacity는 비운다. 언리얼도 풀이 아닌 항목은 비어 있다.
		sprintf_s(Row.Cells[2], "%s", "");
		sprintf_s(Row.Cells[3], "%s", "Physical");
		sprintf_s(Row.Cells[4], "%s", "");

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
		sprintf_s(TotalRow.Cells[0], "%s", "Total");
		sprintf_s(TotalRow.Cells[1], "%6.2f MB", TotalBytes / (1024.0 * 1024.0));
		sprintf_s(TotalRow.Cells[2], "%s", "");
		sprintf_s(TotalRow.Cells[3], "%s", "Physical");
		sprintf_s(TotalRow.Cells[4], "%s", "");
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
