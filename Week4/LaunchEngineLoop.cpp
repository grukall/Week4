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
#include "FStatManager.h"

#include "Material.h"
void FEngineLoop::Init(HINSTANCE hInstance, WNDPROC WndProc, const char* CmdLine)
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

	// 아래 초기화들이 리소스를 만들면서 INC_MEMORY_STAT_BY 같은 매크로를 타는데, 그때 이미 살아 있어야 한다.
	InitStatManager();

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

	FrameTimer = new FFrameTimer(false);
	//ViewportClient = new FEditorViewportClient(*mGraphicsManager->GetRenderer()); // Todo: cChange to class

	ViewportClients.Empty();
	for (int i = 0; i < 4; ++i)
	{
		FEditorViewportClient* NewClient = new FEditorViewportClient(*mGraphicsManager->GetRenderer());

		// @TODO : 해상도 관련 문제
		NewClient->mRenderTarget = mGraphicsManager->GetRenderer()->CreateRenderTarget2D(800, 600, DXGI_FORMAT_R8G8B8A8_UNORM);
		NewClient->mDepthStencil = mGraphicsManager->GetRenderer()->CreateDepthStencil(800, 600);

		ViewportClients.Add(NewClient);
	}
	ViewportClients[0]->SetViewportType(EViewportType::Top);
	ViewportClients[0]->ViewMode = EViewModeIndex::VMI_Wireframe;

	ViewportClients[1]->SetViewportType(EViewportType::Perspective);
	ViewportClients[1]->ViewMode = EViewModeIndex::VMI_Lit;

	ViewportClients[2]->SetViewportType(EViewportType::Front);
	ViewportClients[2]->ViewMode = EViewModeIndex::VMI_Wireframe;

	ViewportClients[3]->SetViewportType(EViewportType::Right);
	ViewportClients[3]->ViewMode = EViewModeIndex::VMI_Wireframe;

	ActiveViewportClient = ViewportClients[1];

	const FVector4 NearTint(1.0f, 0.65f, 0.15f, 0.85f); // 주황 = 가까운 쪽
	const FVector4 FarTint(0.25f, 0.55f, 1.0f, 0.85f); // 파랑 = 먼 쪽

	
	mFileManager = new FFileManager();
	mFontManager = new FFontManager();
	mComponentVisualizerManager = new FComponentVisualizerManager();
	InitAssetManager();

	URenderer* renderer = mGraphicsManager->GetRenderer();
	mThumbnailManager = new FThumbnailManager();
	mThumbnailManager->Initialize(renderer->GetDevice(),renderer->GetDeviceContext(), mAssetManager, mGraphicsManager);
	mSceneManager = new FSceneManager(ViewportClients, mThumbnailManager);

	char Value[64] = {};
	GetPrivateProfileStringA("Grid", "Gap", "", Value, sizeof(Value), ".\\editor.ini");
	int32 GridGap = 1;
	sscanf_s(Value, "%d", &	GridGap);
	mGraphicsManager->SetGridGap(GridGap);
#if IS_OBJ_VIEWER
	mSceneManager->InitObjViewer(CmdLine);
#else
	mSceneManager->NewScene();
#endif
}

void FEngineLoop::InitAssetManager()
{
	mAssetManager = new FAssetManager();

	URenderer* renderer = mGraphicsManager->GetRenderer();

	UMaterial::InitDefaultMaterial(renderer);

	// 엔진 내장 프리미티브도 obj로 임포트한 메시와 같은 .uasset 파이프라인을 태운다.
	// (Import()가 아니라 ImportPrimitive() — obj 소스가 없어서 재임포트 판단이 필요 없다.)
	FStaticMeshAssetLoader* PrimitiveLoader = mAssetManager->GetOrCreateLoader<FStaticMeshAssetLoader>(*renderer, *mAssetManager);

	auto ImportPrimitiveMesh = [&](const FName& Name, const std::filesystem::path& SourceFilePath, const FVertexSimple* Vertices, size_t VertexBytes, const uint32* Indices, size_t IndexBytes) -> UStaticMesh*
	{
		FName Key = PrimitiveLoader->ImportPrimitive(Name, SourceFilePath, *mFileManager, Vertices, static_cast<uint32>(VertexBytes / sizeof(FVertexSimple)), Indices, static_cast<uint32>(IndexBytes / sizeof(uint32)));
		return mAssetManager->GetAssetAs<UStaticMesh>(Key, true);
	};

	UStaticMesh* cubeAsset = ImportPrimitiveMesh(FName("CubeMesh"), "Week4/Cube.h", Cube_vertices, sizeof(Cube_vertices), Cube_indices, sizeof(Cube_indices));
	cubeAsset->SetMaterial(0, UMaterial::DefaultMaterial);

	UStaticMesh* sphereAsset = ImportPrimitiveMesh(FName("SphereMesh"), "Week4/Sphere.h", Sphere_vertices, sizeof(Sphere_vertices), Sphere_indices, sizeof(Sphere_indices));
	sphereAsset->SetMaterial(0, UMaterial::DefaultMaterial);

	UStaticMesh* circleAsset = ImportPrimitiveMesh(FName("CircleMesh"), "Week4/Circle.h", Circle_vertices, sizeof(Circle_vertices), Circle_indices, sizeof(Circle_indices));
	circleAsset->SetMaterial(0, UMaterial::DefaultMaterial);

	UStaticMesh* triangleAsset = ImportPrimitiveMesh(FName("TriangleMesh"), "Week4/Triangle.h", Triangle_vertices, sizeof(Triangle_vertices), Triangle_indices, sizeof(Triangle_indices));
	triangleAsset->SetMaterial(0, UMaterial::DefaultMaterial);

	UStaticMesh* gizmoArrowAsset = ImportPrimitiveMesh(FName("GizmoArrowMesh"), "Week4/GizmoArrow.h", GizmoArrow_vertices, sizeof(GizmoArrow_vertices), GizmoArrow_indices, sizeof(GizmoArrow_indices));
	gizmoArrowAsset->SetMaterial(0, UMaterial::DefaultMaterial);

	UStaticMesh* PlaneAsset = ImportPrimitiveMesh(FName("PlaneMesh"), "Week4/Plane.h", Plane_vertices, sizeof(Plane_vertices), Plane_indices, sizeof(Plane_indices));
	PlaneAsset->SetMaterial(0, UMaterial::DefaultMaterial);

	// 사용자가 obj를 임포트해서 만든 .uasset들(프리미티브는 위에서 이미 등록됐으니 건너뛴다)을
	// 로드 없이 미리 등록해서, 이번 세션에서 한 번도 안 불러온 것도 에셋 드롭다운에 뜨게 한다.
	// (ScanBakedAssets가 FAssetRegistry::ScanDirectory를 먼저 돌려 GUID -> 경로 인덱스를 채운다.)
	mAssetManager->ScanBakedAssets("Assets/Baked", *renderer, *mFileManager);

	FFileAssetSource* FileAssetSource = new FFileAssetSource(*mFileManager, "Textures/Test.jpg");
	mAssetManager->RegisterAsset<FTexture2DAssetLoader>(FName("TestTexture"), FileAssetSource, *renderer);

	FFileAssetSource* SpotLightIconAssetSource = new FFileAssetSource(*mFileManager, "Textures/Icon_SpotLight.png");
	mAssetManager->RegisterAsset<FTexture2DAssetLoader>(FName("SpotLightIcon"), SpotLightIconAssetSource, *renderer);

	FFileAssetSource* ExplosionTextureSource = new FFileAssetSource(*mFileManager, "Textures/ExplosionAtlas.png");
	mAssetManager->RegisterAsset<FTexture2DAssetLoader>(FName("ExplosionTexture"), ExplosionTextureSource, *renderer);

	UTexture2D* ExplosionTexture2DAsset = mAssetManager->GetAssetAs<UTexture2D>("ExplosionTexture", true);
	USpriteAtlas* ExplosionSpriteAtlasAsset = FObjectFactory::ConstructObject<USpriteAtlas> (FName("ExplosionSpriteAtlas"), *renderer, ExplosionTexture2DAsset, 6, 6);
	mAssetManager->RegisterAsset(ExplosionSpriteAtlasAsset);

	FFileAssetSource* FontAssetSource = new FFileAssetSource(*mFileManager, "Fonts/BMKkubulimTTF.ttf");
	mAssetManager->RegisterAsset<FFontAssetLoader>(FName("TestFont"), FontAssetSource, *mFontManager);
	
	UFont* TestFontAsset = mAssetManager->GetAssetAs<UFont>(FName("TestFont"), true);
	UFontAtlas* FontAtlasAsset = FObjectFactory::ConstructObject<UFontAtlas>(FName("TestFontAtlas"), *renderer, TestFontAsset, 512, 512, 2, 2);
	mAssetManager->RegisterAsset(FontAtlasAsset);

	// 스탯 HUD용 고정폭 폰트. 숫자가 바뀌어도 글자 폭이 같아야 표가 흔들리지 않는다.
	FFileAssetSource* StatFontSource = new FFileAssetSource(*mFileManager, "Fonts/RobotoMono-Regular.ttf");
	mAssetManager->RegisterAsset<FFontAssetLoader>(FName("StatFont"), StatFontSource ,*mFontManager);

	UFont* StatFontAsset = mAssetManager->GetAssetAs<UFont>(FName("StatFont"), true);
	UFontAtlas* StatFontAtlasAsset = FObjectFactory::ConstructObject<UFontAtlas>(FName("StatFontAtlas"), *renderer, StatFontAsset, 512, 512, 2, 2);
	mAssetManager->RegisterAsset(StatFontAtlasAsset);
}      

void FEngineLoop::InitStatManager()
{
	mStatManager = new FStatManager();

	// 매크로를 처음 지날 때 자동 등록되지만, 아직 한 번도 안 지난 스탯은
	// 콘솔 목록에 뜨지 않는다. 미리 등록해 목록을 고정해둔다.

	// --- Cycle: 구간별 소요 시간(ms) ---
	mStatManager->Register(FName("Frame"), EStatType::Cycle);
	mStatManager->Register(FName("Game"), EStatType::Cycle);
	mStatManager->Register(FName("Draw"), EStatType::Cycle);
	mStatManager->Register(FName("ImGui"), EStatType::Cycle);   // Draw에 포함된 시간 중 ImGui 몫
	mStatManager->Register(FName("GPU Time"), EStatType::Cycle);
	mStatManager->Register(FName("Input"), EStatType::Cycle);

	// --- Counter: 프레임당 개수 ---
	mStatManager->Register(FName("Draws"), EStatType::Counter);
	mStatManager->Register(FName("Prims"), EStatType::Counter);

	// --- Memory: 현재 총량(byte). 프레임마다 리셋되지 않는다 ---
	mStatManager->Register(FName("VertexBufferMem"), EStatType::Memory);
	mStatManager->Register(FName("IndexBufferMem"), EStatType::Memory);
	mStatManager->Register(FName("TextureMem"), EStatType::Memory);
}

void FEngineLoop::Tick(bool bPumpMessages)
{
	if (GInTick) return;
	GInTick = true;

	mStatManager->ResetFrame();
	FrameTimer->StartFrame();
	SCOPE_CYCLE_COUNTER("Frame");
	float deltaTime = FrameTimer->GetDeltaTime();

	// 공통 접근 변수 및 입력 상태 1회 초기화
	ConsoleWindow& console = ConsoleWindow::Get();
	FRenderCollector& RenderCollector = mGraphicsManager->GetRenderCollector();
	const FInputState& Input = WindowApplication.Input;
#if IS_OBJ_VIEWER
	if (!ViewportClients.IsEmpty()) {
		ActiveViewportClient = ViewportClients[0];
	}
#endif
	RenderCollector.Camera = &ActiveViewportClient->GetCamera();

	// Input Threads & Active Viewport Update
	WindowApplication.ProcessDeferredEvents();
	mGraphicsManager->UpdateProjectionTransition(deltaTime);
	const float ActivePerspectiveRatio = ActiveViewportClient->GetPerspectiveRatio(mGraphicsManager->GetPerspectiveRatio());
	ActiveViewportClient->Update(deltaTime, mSceneManager, ActivePerspectiveRatio, RenderCollector);
	if (WindowApplication.Input.WasPressed(VK_OEM_3))
	{
		console.RequestFocus();
	}

	// Game Threads
	{
		SCOPE_CYCLE_COUNTER("Game");

		mSceneManager->Tick(deltaTime);
		mSceneManager->Update(deltaTime, RenderCollector);
	}

	const float ActiveAspect = static_cast<float>(ActiveViewportClient->mWidth) / static_cast<float>(ActiveViewportClient->mHeight);
	const FMatrix ActiveViewProjMatrix =
		ActiveViewportClient->GetCamera().GetViewMatrix() *
		ActiveViewportClient->GetCamera().GetUnifiedProjectionMatrix(ActiveAspect, ActiveViewportClient->GetCamera().mFovDegree, ActiveViewportClient->GetCamera().mOrthoDistance, 0.1f, 1000.f, ActivePerspectiveRatio);


	// Mouse Picking & Gizmo
	{
		// 피킹 로직
		AActor* HitActor = ActiveViewportClient->PerformMousePicking(ActivePerspectiveRatio, RenderCollector, *mSceneManager);

		if (mSceneManager->IsViewportHovered() && Input.WasPressed(VK_LBUTTON) &&
			!ActiveViewportClient->mGizmo.IsDragging() && !ActiveViewportClient->mGizmo.IsMouseOverHandle())
		{
			if (HitActor) mSceneManager->SetSelectedActor(HitActor);
			else          mSceneManager->ResetSelectedActor();
		}

		// 선택된 액터 AABB 라인 렌더링
		if (AActor* SelectedActor = mSceneManager->GetSelectedActor())
		{
			FTransform Transform = SelectedActor->GetTransform();
			for (UActorComponent* Component : SelectedActor->GetComponents())
			{
				if (UStaticMeshComponent* PrimitiveComponent = Component->Cast<UStaticMeshComponent>())
				{
					if (UStaticMesh* MeshAsset = PrimitiveComponent->GetStaticMesh())
					{
						FMatrix WorldMatrix = Transform.MakeMatrix();
						const FAABB& AABB = MeshAsset->GetLocalBoundingBox().ToWorld(WorldMatrix);

						AABB.ForEachCornerLines([&RenderCollector](const FVector& Start, const FVector& End)
							{
								FRenderLineInfo LineInfo;
								LineInfo.Start = FVector4(Start, 1.f).ToVec3();
								LineInfo.End = FVector4(End, 1.f).ToVec3();
								LineInfo.Color = FVector4(1.f, 0.f, 0.f, 1.f);
								LineInfo.Thickness = 5.0f;
								RenderCollector.LineInfos.Add(LineInfo);
							});
					}
				}

				if (FComponentVisualizer* Visualizer = mComponentVisualizerManager->FindVisualizer(Component->GetRuntimeClass()))
				{
					Visualizer->VisualizeComponent(Component, RenderCollector);
				}
			}
		}

		// Gizmo Update
		const float ActiveViewportX = mSceneManager->GetViewportX() + ActiveViewportClient->mViewportLeft;
		const float ActiveViewportY = mSceneManager->GetViewportY() + ActiveViewportClient->mViewportTop;

		ActiveViewportClient->mGizmo.Update(
			mSceneManager,
			ActiveViewProjMatrix,
			ActiveViewportX,
			ActiveViewportY,
			static_cast<float>(ActiveViewportClient->mWidth),
			static_cast<float>(ActiveViewportClient->mHeight)
		);
	}

	// Render Threads
	{
		SCOPE_CYCLE_COUNTER("Draw");
		if (WindowApplication.bPendingResize)
		{
			mGraphicsManager->OnResize(WindowApplication.PendingWidth, WindowApplication.PendingHeight);
			WindowApplication.bPendingResize = false;
		}

		mGraphicsManager->Update(deltaTime);

#if IS_OBJ_VIEWER
		if (!ViewportClients.IsEmpty()) {
			FEditorViewportClient* CurrentClient = ViewportClients[0];

			CurrentClient->ResizeRenderTarget(mGraphicsManager);

			mGraphicsManager->GetRenderer()->BindRenderTarget(CurrentClient->mRenderTarget, CurrentClient->mDepthStencil, true);

			const float currentWidth = static_cast<float>(CurrentClient->mWidth);
			const float currentHeight = static_cast<float>(CurrentClient->mHeight);

			mGraphicsManager->Prepare(&CurrentClient->mCamera, currentWidth, currentHeight);

			if (UFontAtlas* StatFontAtlas = mAssetManager->GetAssetAs<UFontAtlas>(FName("StatFontAtlas")))
			{
				CurrentClient->DrawStatsHUD(CurrentClient->StatCommands, StatFontAtlas,
					mGraphicsManager->GetRenderCollector(), currentWidth, currentHeight);
			}

			mGraphicsManager->FlushLines();
			mGraphicsManager->Render();
			mGraphicsManager->GetRenderCollector().ClearScreenQuads();
		}
#else

		// 4개의 뷰포트 드로우콜
		for (int i = 0; i < 4; ++i)
		{
			FEditorViewportClient* CurrentClient = ViewportClients[i];

			CurrentClient->ResizeRenderTarget(mGraphicsManager);

			// 렌더 타겟 바인딩 및 Clear
			mGraphicsManager->GetRenderer()->BindRenderTarget(CurrentClient->mRenderTarget, CurrentClient->mDepthStencil, true);

			const float currentWidth = static_cast<float>(CurrentClient->mWidth);
			const float currentHeight = static_cast<float>(CurrentClient->mHeight);
			const float CurrentRatio = CurrentClient->GetPerspectiveRatio(mGraphicsManager->GetPerspectiveRatio());

			mGraphicsManager->Prepare(&CurrentClient->mCamera, currentWidth, currentHeight, CurrentRatio, CurrentClient->ViewMode);

			if (UFontAtlas* StatFontAtlas = mAssetManager->GetAssetAs<UFontAtlas>(FName("StatFontAtlas")))
			{
				CurrentClient->DrawStatsHUD(CurrentClient->StatCommands, StatFontAtlas,
					mGraphicsManager->GetRenderCollector(), currentWidth, currentHeight);
			}

			mGraphicsManager->FlushLines();
			mGraphicsManager->Render();
			mGraphicsManager->GetRenderCollector().ClearScreenQuads();

			if (mSceneManager->GetSelectedActor())
			{
				FRenderInfo clickedRenderInfo;
				mSceneManager->GetSelectedActor()->GetFirstRenderInfo(clickedRenderInfo);
				mGraphicsManager->RenderHighLight(clickedRenderInfo);
			}

			// 각 뷰포트별 렌더링용 ViewProj 계산 및 기즈모 렌더링
			const float Aspect = currentWidth / currentHeight;
			const FMatrix CurrentViewProj = CurrentClient->mCamera.GetViewMatrix() *
				CurrentClient->mCamera.GetUnifiedProjectionMatrix(Aspect, CurrentClient->mCamera.mFovDegree, CurrentClient->mCamera.mOrthoDistance, 0.1f, 1000.f, CurrentRatio);

			CurrentClient->mGizmo.Render(
				mSceneManager,
				CurrentClient->mCamera.Transform.Location,
				CurrentViewProj,
				currentWidth,
				currentHeight
			);
		}
#endif

		// ImGui
		SCOPE_CYCLE_COUNTER("ImGui");
		mSceneManager->UpdateGUI({ *FrameTimer, mGraphicsManager, ActiveViewportClient, mFileManager, mAssetManager });
		mGraphicsManager->GetRenderer()->BindFrameBuffer();
		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		mGraphicsManager->Display();
	}

	mGraphicsManager->GetRenderCollector().Clear();

	//입력 지연 시간 측정(Stat Unit의 Input)
	static double LastInputLatencyMs = 0;
	if (WindowApplication.bHasPendingInput)
	{
		LARGE_INTEGER Now; QueryPerformanceCounter(&Now);
		LastInputLatencyMs = (Now.QuadPart - WindowApplication.PendingInputTime.QuadPart) * GetMsPerCount();
		WindowApplication.bHasPendingInput = false;
	}
	SET_CYCLE_COUNTER("Input", LastInputLatencyMs);

	FrameTimer->EndFrame();
	GInTick = false;
}

void FEngineLoop::End()
{
	const std::string Value = std::format("{:.6f}", ActiveViewportClient->GetCamera().Sensitivity);

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

	if (mThumbnailManager) {
		mThumbnailManager->Shutdown();
		delete mThumbnailManager;
		mThumbnailManager = nullptr;
	}

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
