#include "LaunchEngineLoop.h"

#include <windows.h>
#include "Renderer.h"
#include "WindowApplication.h"
#include "Console.h"
#include "GraphicsManager.h"
#include "CubeComponent.h"
#include "UStaticMeshComponent.h"
#include "ObjectFactory.h"
#include "Cube.h"
#include "Sphere.h"
#include "Circle.h"
#include "Triangle.h"
#include "Plane.h"
#include "Object.h"
#include "GizmoArrow.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "imGui/imgui_impl_win32.h"
#include "Actor.h"
#include "World.h"
#include <FLogManager.h>
#include "Assets.h"

void FEngineLoop::Init(HINSTANCE hInstance, WNDPROC WndProc)
{
	// Initialize window infos
	WCHAR WindowClass[] = L"JungleWindowClass";
	WCHAR Title[] = L"Game Tech Lab";
	WNDCLASSW wndclass = { 0, WndProc, 0, 0, 0, 0, 0, 0, 0, WindowClass };
	RegisterClassW(&wndclass);

	HWND hWnd = CreateWindowExW(
		0,
		WindowClass,
		Title,
		WS_VISIBLE | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 600, 1024,
		nullptr, nullptr, hInstance, nullptr
	);

	// 창을 화면 크기에 맞게 최대화하여 표시
	ShowWindow(hWnd, SW_SHOWMAXIMIZED);
	UpdateWindow(hWnd);

	// 최대화된 후의 실제 클라이언트 크기를 구해 콘솔에 전달
	RECT clientRect;
	GetClientRect(hWnd, &clientRect);
	int clientWidth = clientRect.right - clientRect.left;
	int clientHeight = clientRect.bottom - clientRect.top;

	RAWINPUTDEVICE rid = {};
	rid.usUsagePage = 0x01;		// Generic Desktop
	rid.usUsage = 0x02;			// Mouse
	rid.dwFlags = 0;		// 포커스 있을 때만 수신
	rid.hwndTarget = hWnd;
	RegisterRawInputDevices(&rid, 1, sizeof(rid));

	mGraphicsManager = new FGraphicsManager(hWnd);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init((void*)hWnd);
	ImGui_ImplDX11_Init(mGraphicsManager->GetRenderer()->GetDevice(), mGraphicsManager->GetRenderer()->GetDeviceContext());
	auto& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	IO.Fonts->AddFontFromFileTTF(
		"C:/Windows/Fonts/malgun.ttf",
		18.0f,
		nullptr,
		IO.Fonts->GetGlyphRangesKorean()
	);

	/* Console Window */
	ConsoleWindow& console = ConsoleWindow::Get();
	console.Init(clientWidth);

	FrameTimer = new FFrameTimer(120);

	ViewportClients.Empty();
	for (int i = 0; i < 4; ++i)
	{
		FEditorViewportClient* NewClient = new FEditorViewportClient(*mGraphicsManager->GetRenderer());

		// @TODO : 해상도 관련 문제
		NewClient->mRenderTarget = mGraphicsManager->GetRenderer()->CreateRenderTarget2D(800, 600, DXGI_FORMAT_R8G8B8A8_UNORM);
		NewClient->mDepthStencil = mGraphicsManager->GetRenderer()->CreateDepthStencil(800, 600);

		ViewportClients.Add(NewClient);
	}
	

	const FVector4 NearTint(1.0f, 0.65f, 0.15f, 0.85f); // 주황 = 가까운 쪽
	const FVector4 FarTint(0.25f, 0.55f, 1.0f, 0.85f); // 파랑 = 먼 쪽

	mSceneManager = new FSceneManager();
	mFileManager = new FFileManager();
	mFontManager = new FFontManager();
	InitAssetManager();

	mComponentVisualizerManager = new FComponentVisualizerManager();

	char Value[64] = {};
	GetPrivateProfileStringA("Grid", "Gap", "", Value, sizeof(Value), ".\\editor.ini");
	int32 GridGap = 1;
	sscanf_s(Value, "%d", &	GridGap);
	mGraphicsManager->SetGridGap(GridGap);

	mSceneManager->NewScene();

	//test code
	//{
	//	UCubeComponent* cubeComonent = FObjectFactory::ConstructObject<UCubeComponent>(FVector(0), FRotator(), FVector(1));
	//	AActor* cubeActor = FObjectFactory::ConstructObject<AActor>();
	//	cubeActor->AddComponent(cubeComonent);
	//	mSceneManager.GetCurrentWorld()->AddActor(cubeActor);
	//}
}

void FEngineLoop::InitAssetManager()
{
	mAssetManager = new FAssetManager();

	URenderer* renderer = mGraphicsManager->GetRenderer();
	
	// Register built-in asset types
	UStaticMesh* cubeAsset = FObjectFactory::ConstructObject<UStaticMesh>(FName("CubeMesh"), *renderer, Cube_vertices, sizeof(Cube_vertices) / sizeof(FVertexSimple), Cube_indices, sizeof(Cube_indices) / sizeof(uint32));
	mAssetManager->RegisterAsset(cubeAsset);

	UStaticMesh* sphereAsset = FObjectFactory::ConstructObject<UStaticMesh>(FName("SphereMesh"), *renderer, Sphere_vertices, sizeof(Sphere_vertices) / sizeof(FVertexSimple), Sphere_indices, sizeof(Sphere_indices) / sizeof(uint32));
	mAssetManager->RegisterAsset(sphereAsset);

	UStaticMesh* circleAsset = FObjectFactory::ConstructObject<UStaticMesh>(FName("CircleMesh"), *renderer, Circle_vertices, sizeof(Circle_vertices) / sizeof(FVertexSimple), Circle_indices, sizeof(Circle_indices) / sizeof(uint32));
	mAssetManager->RegisterAsset(circleAsset);

	UStaticMesh* triangleAsset = FObjectFactory::ConstructObject<UStaticMesh>(FName("TriangleMesh"), *renderer, Triangle_vertices, sizeof(Triangle_vertices) / sizeof(FVertexSimple), Triangle_indices, sizeof(Triangle_indices) / sizeof(uint32));
	mAssetManager->RegisterAsset(triangleAsset);

	UStaticMesh* gizmoArrowAsset = FObjectFactory::ConstructObject<UStaticMesh>(FName("GizmoArrowMesh"), *renderer, GizmoArrow_vertices, sizeof(GizmoArrow_vertices) / sizeof(FVertexSimple), GizmoArrow_indices, sizeof(GizmoArrow_indices) / sizeof(uint32));
	mAssetManager->RegisterAsset(gizmoArrowAsset);

	UStaticMesh* PlaneAsset = FObjectFactory::ConstructObject<UStaticMesh>(FName("PlaneMesh"), *renderer, Plane_vertices, sizeof(Plane_vertices) / sizeof(FVertexSimple), Plane_indices, sizeof(Plane_indices) / sizeof(uint32));
	mAssetManager->RegisterAsset(PlaneAsset);

	FTexture2DAssetLoader* TextureLoader = new FTexture2DAssetLoader(*renderer);
	FFontAssetLoader* FontLoader = new FFontAssetLoader(*mFontManager);

	FFileAssetSource* FileAssetSource = new FFileAssetSource(*mFileManager, "Textures/Test.jpg");
	mAssetManager->RegisterAsset(FName("TestTexture"), TextureLoader, FileAssetSource);

	FFileAssetSource* SpotLightIconAssetSource = new FFileAssetSource(*mFileManager, "Textures/Icon_SpotLight.png");
	mAssetManager->RegisterAsset(FName("SpotLightIcon"), TextureLoader, SpotLightIconAssetSource);

	FFileAssetSource* ExplosionTextureSource = new FFileAssetSource(*mFileManager, "Textures/ExplosionAtlas.png");
	mAssetManager->RegisterAsset(FName("ExplosionTexture"), TextureLoader, ExplosionTextureSource);

	UTexture2D* ExplosionTexture2DAsset = mAssetManager->GetAssetAs<UTexture2D>("ExplosionTexture", true);
	USpriteAtlas* ExplosionSpriteAtlasAsset = FObjectFactory::ConstructObject<USpriteAtlas>(FName("ExplosionSpriteAtlas"), *renderer, ExplosionTexture2DAsset, 6, 6);
	mAssetManager->RegisterAsset(ExplosionSpriteAtlasAsset);

	FFileAssetSource* FontAssetSource = new FFileAssetSource(*mFileManager, "Fonts/BMKkubulimTTF.ttf");
	mAssetManager->RegisterAsset(FName("TestFont"), FontLoader, FontAssetSource);
	
	UFont* TestFontAsset = mAssetManager->GetAssetAs<UFont>(FName("TestFont"), true);
	UFontAtlas* FontAtlasAsset = FObjectFactory::ConstructObject<UFontAtlas>(FName("TestFontAtlas"), *renderer, TestFontAsset, 512, 512, 2, 2);
	mAssetManager->RegisterAsset(FontAtlasAsset);
}

void FEngineLoop::Tick(bool bPumpMessages)
{
	if (GInTick) return;
	GInTick = true;

	FrameTimer->StartFrame();
	float deltaTime = FrameTimer->GetDeltaTime();
	ConsoleWindow& console = ConsoleWindow::Get();

	FRenderCollector& RenderCollector = mGraphicsManager->GetRenderCollector();

	// 피킹/컬링 등의 기준은 일단 0번 뷰포트의 카메라로 고정
	RenderCollector.Camera = &ViewportClients[0]->GetCamera();

	// Input Threads
	{
		WindowApplication.ProcessDeferredEvents();

		mGraphicsManager->UpdateProjectionTransition(deltaTime);

		for (int i = 0; i < 4; ++i)
		{
			ViewportClients[i]->Update(deltaTime, mSceneManager, mGraphicsManager->GetPerspectiveRatio(), RenderCollector);
		}
	}

	// Physics / Game Threads (씬 로직은 화면 갯수와 무관하게 1번만)
	{
		mSceneManager->Tick(deltaTime);
		mSceneManager->Update(deltaTime, RenderCollector);
	}

	// Mouse Picking & Gizmo (0번 뷰포트 기준으로 고정)
	{
		const FInputState& Input = WindowApplication.Input;

		// 모든 ViewportClient를 ViewportClients[0]으로 변경
		AActor* HitActor = ViewportClients[0]->PerformMousePicking(mGraphicsManager->GetPerspectiveRatio(), RenderCollector, *mSceneManager);
		if (mSceneManager->IsViewportHovered() && Input.WasPressed(VK_LBUTTON) && !ViewportClients[0]->mGizmo.IsDragging() && !ViewportClients[0]->mGizmo.IsMouseOverHandle())
		{
			if (HitActor) mSceneManager->SetSelectedActor(HitActor);
			else          mSceneManager->ResetSelectedActor();
		}

		AActor* SelectedActor = mSceneManager->GetSelectedActor();
		if (SelectedActor)
		{
			FTransform Transform = SelectedActor->GetTransform();
			for (UActorComponent* Component : SelectedActor->GetComponents())
			{
				UStaticMeshComponent* PrimitiveComponent = Component->Cast<UStaticMeshComponent>();
				if (PrimitiveComponent)
				{
					FMatrix WorldMatrix = Transform.MakeMatrix();
					UStaticMesh* MeshAsset = PrimitiveComponent->GetStaticMesh();
					if (!MeshAsset) continue;

					const FAABB& AABB = MeshAsset->GetLocalBoundingBox().ToWorld(WorldMatrix);
					AABB.ForEachCornerLines([&RenderCollector](const FVector& Start, const FVector& End)
						{
							FVector4 WorldStart = FVector4(Start, 1.f);
							FVector4 WorldEnd = FVector4(End, 1.f);

							FRenderLineInfo LineInfo;
							LineInfo.Start = WorldStart.ToVec3();
							LineInfo.End = WorldEnd.ToVec3();
							LineInfo.Color = FVector4(1.f, 0.f, 0.f, 1.f);
							LineInfo.Thickness = 5.0f;

							RenderCollector.LineInfos.Add(LineInfo);
						});
				}

				FComponentVisualizer* Visualizer = mComponentVisualizerManager->FindVisualizer(Component->GetRuntimeClass());
				if (Visualizer) Visualizer->VisualizeComponent(Component, RenderCollector);
			}
		}

		ViewportClients[0]->mGizmo.Update(mSceneManager, mGraphicsManager->GetViewProjectionMatrix());
	}

	// Render Threads
	{
		if (WindowApplication.bPendingResize)
		{
			mGraphicsManager->OnResize(WindowApplication.PendingWidth, WindowApplication.PendingHeight);
			WindowApplication.bPendingResize = false;
		}

		mGraphicsManager->Update(deltaTime);

		// 4번의 드로우콜
		for (int i = 0; i < 4; ++i)
		{
			FEditorViewportClient* CurrentClient = ViewportClients[i];

			// @TODO : 해상도 변경 등
			// CurrentClient->ResizeRenderTargetIfNeeded(mGraphicsManager);

			// 현재 클라이언트 전용 렌더 타겟(RT) 바인딩 및 Clear
			mGraphicsManager->GetRenderer()->BindRenderTarget(
				CurrentClient->mRenderTarget,
				CurrentClient->mDepthStencil,
				true
			);

			// TODO : 각 뷰포트의 실제 분할된 해상도 크기를 가져와야 함 (현재는 전체 크기 고정)
			float currentWidth = mSceneManager->GetViewportWidth();   // 추후 스플리터 영역 크기로 변경
			float currentHeight = mSceneManager->GetViewportHeight(); // 추후 스플리터 영역 크기로 변경

			mGraphicsManager->Prepare(&CurrentClient->mCamera, currentWidth, currentHeight);
			mGraphicsManager->FlushLines();
			mGraphicsManager->Render();

			if (mSceneManager->GetSelectedActor())
			{
				FRenderInfo clickedRenderInfo;
				mSceneManager->GetSelectedActor()->GetFirstRenderInfo(clickedRenderInfo);
				mGraphicsManager->RenderHighLight(clickedRenderInfo);
			}

			// 기즈모 렌더링 (4개 화면 모두에서 기즈모가 보이도록 CurrentClient 기준 렌더링)
			// mGraphicsManager->GetViewProjectionMatrix() 도 현재 Prepare된 행렬을 반환해야 정상 출력됨
			CurrentClient->mGizmo.Render(mSceneManager, CurrentClient->mCamera.Transform.Location, mGraphicsManager->GetViewProjectionMatrix());
		}
		mGraphicsManager->GetRenderCollector().Clear();

		// ImGui
		{
			mSceneManager->UpdateGUI({ *FrameTimer, mGraphicsManager, &ViewportClients, ViewportClients[0], mFileManager, mAssetManager });

			mGraphicsManager->GetRenderer()->BindFrameBuffer();

			ImGui::Render();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		}

		mGraphicsManager->Display();
	}

	FrameTimer->EndFrame();
	GInTick = false;
}

void FEngineLoop::End()
{
	const std::string Value = std::format("{:.6f}", ViewportClients[0]->GetCamera().Sensitivity);

	if (!WritePrivateProfileStringA("Camera", "Sensitivity", Value.c_str(), ".\\editor.ini"))
	{
		UE_LOG_ERROR("Failed to save camera sensitivity to editor.ini");
	}

	const std::string ValueGrid = std::format("{:6d}", mGraphicsManager->GetGridGap());

	if (!WritePrivateProfileStringA("Grid", "Gap", ValueGrid.c_str(), ".\\editor.ini"))
	{
		UE_LOG_ERROR("Failed to save grid gap to editor.ini");
	}
	mSceneManager->DeleteScene();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	delete mComponentVisualizerManager;
	for (FEditorViewportClient* Client : ViewportClients)
	{
		delete Client;
	}
	ViewportClients.Empty();
	delete FrameTimer;
	delete mSceneManager;
	delete mFileManager;
	delete mAssetManager;
	delete mFontManager;

	delete mGraphicsManager;
}
