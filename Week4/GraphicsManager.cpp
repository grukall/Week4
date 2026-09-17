#include "GraphicsManager.h"
#include "Renderer.h"
#include "Camera.h"
#include "Console.h"
#include "FLogManager.h"
#include "FAssetManager.h"
#include "Assets.h"
#include "ObjectFactory.h"
#include "UTextComponent.h"

// 선분 하나당 정점 2개. 축 6개 + 앞으로 붙을 그리드까지 감당할 만큼 잡아둔다
static constexpr uint32 LINE_VERTEX_CAPACITY = 8192;

FGraphicsManager::FGraphicsManager(HWND hWindow) :
	mbPerspectiveProjection(true)
	, mProjectionRatio(1.0f)
{
	mRenderer = new URenderer;
	mRenderer->Create(hWindow);
#if 0
	mRenderer->CreateLineVertexBuffer(LINE_VERTEX_CAPACITY);
#endif

	mAspect = mRenderer->GetWidth() / static_cast<float>(mRenderer->GetHeight());
	mSceneRenderTarget = mRenderer->CreateRenderTarget2D(mRenderer->GetWidth(), mRenderer->GetHeight(), DXGI_FORMAT_R8G8B8A8_UNORM);
	mSceneDepthStencil = mRenderer->CreateDepthStencil(mRenderer->GetWidth(), mRenderer->GetHeight());

	mMeshPipeline = mRenderer->CreateRenderPipeline();
	mMeshPipeline->SetRasterRizerState(D3D11_CULL_BACK, 0, { EViewModeIndex::VMI_Lit, EViewModeIndex::VMI_Wireframe });
	mMeshPipeline->SetDepthStencilState(true, true);
	mMeshPipeline->SetShader("Assets/Shaders/Mesh.hlsl");
	mMeshPipeline->AddConstantBuffer<FConstants>();
	mMeshPipeline->AddConstantBuffer<FMatrix>();
}

FGraphicsManager::~FGraphicsManager()
{
	mMeshPipeline.reset();

#if 0
	mRenderer->ReleaseLineVertexBuffer();
#endif
	mRenderer->Release();

	delete mRenderer;
}

void FGraphicsManager::Prepare(const FCamera* mCamera, float viewportWidth, float viewportHeight)
{
	// Cache view and projection matrices for rendering
	const float nearZ = 0.1f;
	const float farZ = 2000.0f;

	float d = mCamera->mOrthoDistance;
	
	mAspect = viewportWidth / viewportHeight;

	FMatrix view = mCamera->GetViewMatrix();
	FMatrix projection_u_p = mCamera->GetUnifiedProjectionMatrix(mAspect, mCamera->mFovDegree, d, nearZ, farZ, 1.0f);
	FMatrix projection_u_o = mCamera->GetUnifiedProjectionMatrix(mAspect, mCamera->mFovDegree, d, nearZ, farZ, 0.0f);
	FMatrix projection_u = mCamera->GetUnifiedProjectionMatrix(mAspect, mCamera->mFovDegree, d, nearZ, farZ, mProjectionRatio);

	//mViewProjectionMatrix = view * mCamera->GetProjectionMatrix(mAspect, mCamera->mFovDegree, nearZ, farZ);
	mViewMatrix = view;
	mProjectionMatrix = projection_u;
	mViewProjectionMatrix = view * projection_u_p;

	// 뷰 모드를 렌더러에 전달한다. BindPipeline이 드로우마다 이 값을 보고
	// 솔리드/와이어프레임 래스터라이저를 고른다.
	mRenderer->SetViewModeIndex(mViewModeIndex);

	mRenderer->Prepare(view * projection_u);

	float orthoHeight = mCamera->mOrthoHeight;
	float orthoWidth = orthoHeight * mAspect;
	//mViewOrthogonalProjectionMatrix = view * mCamera->GetOrthographicMatrix(orthoWidth, orthoHeight, nearZ, farZ);
	mViewOrthogonalProjectionMatrix = view * projection_u_o;
	mViewUnifiedProjectionMatrix = view * projection_u;

	// 하이라이트 두께를 화면 픽셀 기준으로 환산할 때 쓴다
	mCameraLocation = mCamera->Transform.Location;
	mCameraForward = mCamera->GetForwardVector();
	mCameraFovDegree = mCamera->mFovDegree;
	mCameraOrthoDistance = mCamera->mOrthoDistance;

	// 그리는 순서가 중요하다: 가까운 것을 먼저, 먼 것을 나중에.
	// 깊이 테스트가 켜져 있으면 나중에 그린 FarCube 가 깊이 비교에서 탈락해
	// NearCube(주황)가 앞에 남고, 꺼져 있으면 FarCube(파랑)가 그 위를 덮어쓴다.
	//mRenderer->UpdateConstantViewProjection(viewProjection);

	mRenderer->BindRenderTarget(mSceneRenderTarget, mSceneDepthStencil);
}

void FGraphicsManager::GizmoPrepare()
{
	mRenderer->RSUpdateState();

}
void FGraphicsManager::Render()
{
	mRenderer->RenderLines(mRenderCollector.LineInfos);

	for (const FRenderInfo& renderInfo : mRenderCollector.RenderInfos)
	{
		UStaticMesh* Asset = renderInfo.StaticMesh;

		mMeshPipeline->ClearShaderResource();
		mMeshPipeline->ClearSamplerState();

		if (renderInfo.Texture)
		{
			FConstants Constants{};
			Constants.Matrix = renderInfo.WorldTransformMatrix;
			Constants.Color = FVector4(1, 1, 1, 1);
			Constants.UseVertexColor = 0;
			Constants.HasTexture = 1;

			mMeshPipeline->UpdateConstantBuffer(0, Constants);
			mMeshPipeline->UpdateConstantBuffer(1, mViewUnifiedProjectionMatrix);

			mMeshPipeline->SetShaderResource(0, renderInfo.Texture->GetSRV());
			mMeshPipeline->SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);

			mRenderer->RenderPrimitiveIndexed(mMeshPipeline, Asset->GetVertexBuffer(), Asset->GetIndexBuffer(), Asset->GetIndexCount());
		}
		else
		{
			mRenderer->RenderPrimitiveIndexed(Asset->GetVertexBuffer(), Asset->GetIndexBuffer(), Asset->GetIndexCount(), renderInfo.WorldTransformMatrix);
		}
	}

	for (const FRenderQuadInfo& QuadInfo : mRenderCollector.GetOpaqueQuadInfos())
	{
		mRenderer->RenderQuad(QuadInfo);
	}

	if (FShowFlags::Get().IsEnabled(EShowFlag::Grid))
	{
		// Match the grid's world-space half-width of 0.001.
		mRenderer->RenderWorldAxis(mViewMatrix, mProjectionMatrix, FVector4(0.f, 0.f, 1.f, 1.f), FVector3(0.f, 0.f, 1.f), 0.002f);
		mRenderer->RenderWorldGrid(mViewUnifiedProjectionMatrix, mCameraLocation, GridGap);
	}

	for (const FRenderQuadInfo& QuadInfo : mRenderCollector.GetTransparentQuadInfos())
	{
		mRenderer->RenderQuad(QuadInfo);
	}

	for (const FRenderQuadInfo& QuadInfo : mRenderCollector.GetOverlayQuadInfos())
	{
		mRenderer->RenderQuad(QuadInfo);
	}

	mRenderCollector.Clear();
}

void FGraphicsManager::DrawLine(const FVector& start, const FVector& end, const FVector4& color)
{
	// 월드 좌표 그대로 넣는다. 그래서 그릴 때 World 행렬이 단위행렬이다
	mLineVertices.Add({ start.x, start.y, start.z, color.x, color.y, color.z, color.w });
	mLineVertices.Add({ end.x,   end.y,   end.z,   color.x, color.y, color.z, color.w });
}

void FGraphicsManager::FlushLines()
{
#if 0
	if (mLineVertices.Num() == 0) return;

	// 선분 좌표가 이미 월드 공간이라 World는 단위행렬.
	// Tint.a = 0 이면 셰이더의 lerp가 정점 색을 그대로 통과시킨다
	//if (mbPerspectiveProjection)
	//{
	//	mRenderer->UpdateConstant(FMatrix::Identity, mViewProjectionMatrix, FVector4(0, 0, 0, 0));
	//}
	//else
	//{
	//	mRenderer->UpdateConstant(FMatrix::Identity, mViewOrthogonalProjectionMatrix, FVector4(0, 0, 0, 0));
	//}

	mRenderer->UpdateConstant(FMatrix::Identity, mViewUnifiedProjectionMatrix, FVector4(0, 0, 0, 0));
	mRenderer->RenderLines(&mLineVertices[0], mLineVertices.Num());

	// 안 비우면 매 프레임 누적돼 버퍼가 넘친다. 용량은 유지한 채 개수만 0으로
	mLineVertices.Reset(LINE_VERTEX_CAPACITY);
#endif
}

/*
void GraphicsManager::Render(FTransform worldTransformMatrix, EPrimitive ePrimitive)
{
	mRenderer->UpdateConstant(worldTransformMatrix.MakeMatrix(), mViewProjectionMatrix);

	FBuffer vertexBuffer = mBufferMap[ePrimitive];
	mRenderer->RenderPrimitive(vertexBuffer.Buffer, vertexBuffer.SourceNum);
}
*/

void FGraphicsManager::Display()
{
	mRenderer->SwapBuffer();
}

void FGraphicsManager::Update(float deltaTime)
{
}

bool FGraphicsManager::IsPerspectiveProjection() const
{
	return mbPerspectiveProjection;
}

void FGraphicsManager::SetPerspectiveProjection(bool bPerspectiveProjection)
{
	mbPerspectiveProjection = bPerspectiveProjection;
}

URenderer* FGraphicsManager::GetRenderer() const
{
	assert(mRenderer != nullptr);

	return mRenderer;
}

void FGraphicsManager::OnResize(UINT width, UINT height)
{
	if (width == 0 || height == 0)
	{
		return;
	}

	if (mSceneRenderTarget)
	{
		mSceneRenderTarget = mRenderer->CreateRenderTarget2D(width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
	}
	
	if (mSceneDepthStencil)
	{
		mSceneDepthStencil = mRenderer->CreateDepthStencil(width, height);
	}

	mRenderer->OnResize(width, height);
}

FVector FGraphicsManager::GetPrimitiveCenter(EPrimitive type)
{
	switch (type)
	{
	case EPrimitive::EP_Sphere:	return FVector(0, 0, 0);
	case EPrimitive::EP_Cube:	return FVector(0, 0, 0);
	default:					return FVector(0, 0, 0);
	}
}

// 테두리가 화면에서 차지할 두께(픽셀). 물체 크기와 카메라 거리 어느 쪽에도 영향받지 않는다.
static constexpr float OUTLINE_PIXELS = 3.0f;

// 월드 공간 반지름이 worldHalfExtent인 축을 worldThickness 만큼 키우는 배율
static float GetOutlineAxisScale(float worldHalfExtent, float worldThickness)
{
	if (worldHalfExtent <= SMALL_NUMBER)
	{
		return 1.0f;   // 납작하게 눌린 축은 건드리지 않는다. 안 그러면 배율이 발산한다
	}

	return 1.0f + worldThickness / worldHalfExtent;
}

FVector FGraphicsManager::GetPrimitiveHalfExtent(EPrimitive type)
{
	switch (type)
	{
	case EPrimitive::EP_Sphere:	return FVector(1.0f, 1.0f, 1.0f);
	case EPrimitive::EP_Cube:	return FVector(0.5f, 0.5f, 0.5f);
	default:					return FVector(0.5f, 0.5f, 0.5f);
	}
}

void FGraphicsManager::RenderHighLight(const FRenderInfo& RI)
{
	if (!RI.StaticMesh)
	{
		return;
	}

	const FVector Center = GetPrimitiveCenter(RI.ePrimitive);
	const FVector HalfExtent = GetPrimitiveHalfExtent(RI.ePrimitive);

	// 화면에서 OUTLINE_PIXELS 만큼 보이려면 이 깊이에서 월드로 얼마여야 하는지 환산한다.
	// 깊이 d에서 뷰포트가 담는 월드 높이가 2*d*tan(fov/2) 이므로, 그걸 픽셀 수로 나누면 픽셀당 월드 크기다.
	const FVector ObjectLocation = RI.WorldTransformMatrix.TransformPosition(Center);
	const float Depth = FVector::dot(ObjectLocation - mCameraLocation, mCameraForward);
	const float TanHalfFov = tanf(FMath::DegreesToRadians(mCameraFovDegree * 0.5f));
	const float effectiveDepth = FMath::Max(
		(1.0f - mProjectionRatio) * mCameraOrthoDistance + mProjectionRatio * Depth
		, 0.01f);
	//const float H = mbPerspectiveProjection ? 2.0f * Depth * TanHalfFov : 5.774f;
	const float H = 2.0f * effectiveDepth * TanHalfFov;
	const float WorldThickness = OUTLINE_PIXELS * H / mRenderer->GetHeight();


	// 축마다 월드 공간에서 WorldThickness 만큼만 자라도록 배율을 따로 구한다.
	const FVector WorldScale(
		RI.WorldTransformMatrix.GetUnitAxis(EAxis::X).Length(),
		RI.WorldTransformMatrix.GetUnitAxis(EAxis::Y).Length(),
		RI.WorldTransformMatrix.GetUnitAxis(EAxis::Z).Length());

	FVector OutlineScale = {
		GetOutlineAxisScale(HalfExtent.x * WorldScale.x, WorldThickness),
		GetOutlineAxisScale(HalfExtent.y * WorldScale.y, WorldThickness),
		GetOutlineAxisScale(HalfExtent.z * WorldScale.z, WorldThickness) };


	const FMatrix Outline = FMatrix::Translation(FVector(-Center.x, -Center.y, -Center.z))
		* FMatrix::Scale(OutlineScale)
		* FMatrix::Translation(Center)
		* RI.WorldTransformMatrix;

	mRenderer->RenderHighlight(
		RI.StaticMesh->GetVertexBuffer(), RI.StaticMesh->GetVertexCount(),
		RI.StaticMesh->GetIndexBuffer(), RI.StaticMesh->GetIndexCount(),
		RI.WorldTransformMatrix,
		Outline,
		FVector4(1.f, 0.6f, 0.f, 1.f));
}

void FGraphicsManager::StartProjectionTransition(bool orthographic)
{
	mProjectionStartRatio = mProjectionRatio;
	mProjectionTargetRatio = orthographic ? 0.0f : 1.0f;
	mProjectionElapsed = 0.0f;

	mbProjectionTransitioning = mProjectionStartRatio != mProjectionTargetRatio;
}

bool FGraphicsManager::IsOrthographicTarget() const
{
	return mProjectionTargetRatio == 0.0f;
}

void FGraphicsManager::UpdateProjectionTransition(float deltaTime)
{
	if (!mbProjectionTransitioning)
	{
		return;
	}

	mProjectionElapsed += deltaTime;

	const float u = FMath::Clamp(
		mProjectionElapsed / mProjectionDuration, 0.0f, 1.0f);

	// Smoothstep interpolation for a smoother transition
	const float blend = u * u * (3.0f - 2.0f * u);

	mProjectionRatio = mProjectionStartRatio + (mProjectionTargetRatio - mProjectionStartRatio) * blend;

	if (u >= 1.0f)
	{
		mProjectionRatio = mProjectionTargetRatio;
		mbProjectionTransitioning = false;
	}
}

void FGraphicsManager::SetGridGap(int32 GridGap)
{
	if (GridGap > 75000)
		GridGap = 100000;
	else if (GridGap > 30000)
		GridGap = 50000;
	else if (GridGap > 7500)
		GridGap = 10000;
	else if (GridGap > 3000)
		GridGap = 5000;
	else if (GridGap > 750)
		GridGap = 1000;
	else if (GridGap > 300)
		GridGap = 500;
	else if (GridGap > 75)
		GridGap = 100;
	else if (GridGap > 30)
		GridGap = 50;
	else if (GridGap > 7)
		GridGap = 10;
	else if (GridGap > 3)
		GridGap = 5;
	else
		GridGap = 1;
	this->GridGap = GridGap;
}
