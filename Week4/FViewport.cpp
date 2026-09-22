#include "FViewport.h"
#include "FViewportClient.h"
#include "FEditorViewportClient.h"
#include "GraphicsManager.h"
#include "Renderer.h"
#include "ImGui/imgui.h"
#include <algorithm>

FViewport::FViewport(FViewportClient* InClient)
	: mClient(InClient)
{
	if (mClient)
	{
		if (FEditorViewportClient* EditorClient = dynamic_cast<FEditorViewportClient*>(mClient))
		{
			EditorClient->SetViewport(this);
		}
	}
}

void FViewport::SetClient(FViewportClient* InClient)
{
	mClient = InClient;
	if (mClient)
	{
		if (FEditorViewportClient* EditorClient = dynamic_cast<FEditorViewportClient*>(mClient))
		{
			EditorClient->SetViewport(this);
		}
	}
}

void FViewport::SetViewportArea(float InLeft, float InTop, float InWidth, float InHeight)
{
	mViewportLeft = InLeft;
	mViewportTop = InTop;
	mWidth = std::max<uint32>(1, static_cast<uint32>(InWidth));
	mHeight = std::max<uint32>(1, static_cast<uint32>(InHeight));

	if (FEditorViewportClient* EditorClient = dynamic_cast<FEditorViewportClient*>(mClient))
	{
		EditorClient->SetViewportArea(InLeft, InTop, InWidth, InHeight);
	}
}

void FViewport::ResizeRenderTarget(FGraphicsManager* GraphicsManager)
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

void FViewport::Draw(FGraphicsManager* GraphicsMgr, FSceneManager* SceneMgr)
{
	ResizeRenderTarget(GraphicsMgr);

	if (mClient)
	{
		mClient->Draw(this, GraphicsMgr, SceneMgr);
	}
}

void FViewport::DrawViewportUI(
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

	if (rect.GetWidth() <= 0.0f || rect.GetHeight() <= ToolbarHeight) return;

	const float RenderTop = rect.Top + ToolbarHeight;
	const float RenderHeight = rect.GetHeight() - ToolbarHeight;
	const float RenderWidth = rect.GetWidth();

	// 3D 렌더 영역 크기 갱신
	SetViewportArea(rect.Left, RenderTop, RenderWidth, RenderHeight);

	ImGui::PushID(this);

	// 1. 3D 뷰포트 서피스 이미지 렌더링
	if (mRenderTarget && mRenderTarget->SRV)
	{
		ImGui::SetCursorPos(ImVec2(startCursorPos.x + rect.Left, startCursorPos.y + RenderTop));

		ImTextureID srv = (ImTextureID)(intptr_t)mRenderTarget->SRV.Get();
		ImGui::Image(srv, ImVec2(RenderWidth, RenderHeight));

		// 뷰포트 이미지 영역 클릭 시 ActiveViewport 갱신
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
		{
			if (FEditorViewportClient* EditorClient = dynamic_cast<FEditorViewportClient*>(mClient))
			{
				InOutActiveViewport = EditorClient;
			}
		}
	}

	// 2. 상단 툴바 (Toolbar Bar) 렌더링은 에디터 클라이언트에게 위임
	if (FEditorViewportClient* EditorClient = dynamic_cast<FEditorViewportClient*>(mClient))
	{
		EditorClient->DrawToolbar(this, rect, ViewportIndex, InOutActiveViewport, InOutMaximizedIndex, SelectedActor, startCursorPos, screenCursorPos);
	}

	ImGui::PopID();
}
