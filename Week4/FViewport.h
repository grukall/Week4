#pragma once

#include "SWindow.h"
#include "Core.h"
#include <d3d11.h>
#include "Renderer.h"

class FViewportClient;
struct FEditorViewportClient;
class FGraphicsManager;
class FSceneManager;
class AActor;
struct ImVec2;

class FViewport
{
public:
	explicit FViewport(FViewportClient* InClient = nullptr);
	virtual ~FViewport() = default;

	void SetClient(FViewportClient* InClient);
	FViewportClient* GetClient() const { return mClient; }

	template<typename T>
	T* GetClientAs() const { return dynamic_cast<T*>(mClient); }

	void SetViewportArea(float InLeft, float InTop, float InWidth, float InHeight);
	void ResizeRenderTarget(FGraphicsManager* GraphicsManager);

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

	TSharedPtr<FRenderTarget2D> GetRenderTarget() const { return mRenderTarget; }
	TSharedPtr<FDepthStencil> GetDepthStencil() const { return mDepthStencil; }

	uint32 GetWidth() const { return mWidth; }
	uint32 GetHeight() const { return mHeight; }
	float GetViewportLeft() const { return mViewportLeft; }
	float GetViewportTop() const { return mViewportTop; }
	float GetAspectRatio() const { return (mHeight > 0) ? (static_cast<float>(mWidth) / static_cast<float>(mHeight)) : 1.0f; }

public:
	TSharedPtr<FRenderTarget2D> mRenderTarget;
	TSharedPtr<FDepthStencil> mDepthStencil;

	uint32 mWidth = 800;
	uint32 mHeight = 600;
	float mViewportLeft = 0.0f;
	float mViewportTop = 0.0f;

private:
	FViewportClient* mClient = nullptr;
};
