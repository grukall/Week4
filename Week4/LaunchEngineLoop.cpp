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
#include "UObjectHash.h"

#include "Material.h"
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

	FrameTimer = new FFrameTimer(120);
	ViewportClient = new FEditorViewportClient(*mGraphicsManager->GetRenderer()); // Todo: cChange to class

	const FVector4 NearTint(1.0f, 0.65f, 0.15f, 0.85f); // 주황 = 가까운 쪽
	const FVector4 FarTint(0.25f, 0.55f, 1.0f, 0.85f); // 파랑 = 먼 쪽

	mSceneManager = new FSceneManager();
	mFileManager = new FFileManager();
	mFontManager = new FFontManager();
	mComponentVisualizerManager = new FComponentVisualizerManager();
	InitAssetManager();


	char Value[64] = {};
	GetPrivateProfileStringA("Grid", "Gap", "", Value, sizeof(Value), ".\\editor.ini");
	int32 GridGap = 1;
	sscanf_s(Value, "%d", &	GridGap);
	mGraphicsManager->SetGridGap(GridGap);
	mSceneManager->NewScene();
}

void FEngineLoop::InitAssetManager()
{
	mAssetManager = new FAssetManager();

	URenderer* renderer = mGraphicsManager->GetRenderer();

	UMaterial::InitDefaultMaterial(renderer);

	// Register built-in asset types
	UStaticMesh* cubeAsset = FObjectFactory::ConstructObject<UStaticMesh>(FName("CubeMesh"), *renderer, Cube_vertices, sizeof(Cube_vertices) / sizeof(FVertexSimple), Cube_indices, sizeof(Cube_indices) / sizeof(uint32));
	cubeAsset->SetMaterial(0, UMaterial::DefaultMaterial);
	mAssetManager->RegisterAsset(cubeAsset);

	UStaticMesh* sphereAsset = FObjectFactory::ConstructObject<UStaticMesh>(FName("SphereMesh"), *renderer, Sphere_vertices, sizeof(Sphere_vertices) / sizeof(FVertexSimple), Sphere_indices, sizeof(Sphere_indices) / sizeof(uint32));
	sphereAsset->SetMaterial(0, UMaterial::DefaultMaterial);
	mAssetManager->RegisterAsset(sphereAsset);

	UStaticMesh* circleAsset = FObjectFactory::ConstructObject<UStaticMesh>(FName("CircleMesh"), *renderer, Circle_vertices, sizeof(Circle_vertices) / sizeof(FVertexSimple), Circle_indices, sizeof(Circle_indices) / sizeof(uint32));
	circleAsset->SetMaterial(0, UMaterial::DefaultMaterial);
	mAssetManager->RegisterAsset(circleAsset);

	UStaticMesh* triangleAsset = FObjectFactory::ConstructObject<UStaticMesh>(FName("TriangleMesh"), *renderer, Triangle_vertices, sizeof(Triangle_vertices) / sizeof(FVertexSimple), Triangle_indices, sizeof(Triangle_indices) / sizeof(uint32));
	triangleAsset->SetMaterial(0, UMaterial::DefaultMaterial);
	mAssetManager->RegisterAsset(triangleAsset);

	UStaticMesh* gizmoArrowAsset = FObjectFactory::ConstructObject<UStaticMesh>(FName("GizmoArrowMesh"), *renderer, GizmoArrow_vertices, sizeof(GizmoArrow_vertices) / sizeof(FVertexSimple), GizmoArrow_indices, sizeof(GizmoArrow_indices) / sizeof(uint32));
	gizmoArrowAsset->SetMaterial(0, UMaterial::DefaultMaterial);
	mAssetManager->RegisterAsset(gizmoArrowAsset);

	UStaticMesh* PlaneAsset = FObjectFactory::ConstructObject<UStaticMesh>(FName("PlaneMesh"), *renderer, Plane_vertices, sizeof(Plane_vertices) / sizeof(FVertexSimple), Plane_indices, sizeof(Plane_indices) / sizeof(uint32));
	PlaneAsset->SetMaterial(0, UMaterial::DefaultMaterial);
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
	USpriteAtlas* ExplosionSpriteAtlasAsset = FObjectFactory::ConstructObject<USpriteAtlas> (FName("ExplosionSpriteAtlas"), *renderer, ExplosionTexture2DAsset, 6, 6);
	mAssetManager->RegisterAsset(ExplosionSpriteAtlasAsset);

	FFileAssetSource* FontAssetSource = new FFileAssetSource(*mFileManager, "Fonts/BMKkubulimTTF.ttf");
	mAssetManager->RegisterAsset(FName("TestFont"), FontLoader, FontAssetSource);
	
	UFont* TestFontAsset = mAssetManager->GetAssetAs<UFont>(FName("TestFont"), true);
	UFontAtlas* FontAtlasAsset = FObjectFactory::ConstructObject<UFontAtlas>(FName("TestFontAtlas"), *renderer, TestFontAsset, 512, 512, 2, 2);
	mAssetManager->RegisterAsset(FontAtlasAsset);

	// 스탯 HUD용 고정폭 폰트. 숫자가 바뀌어도 글자 폭이 같아야 표가 흔들리지 않는다.
	FFileAssetSource* StatFontSource = new FFileAssetSource(*mFileManager, "Fonts/RobotoMono-Regular.ttf");
	mAssetManager->RegisterAsset(FName("StatFont"), FontLoader, StatFontSource);

	UFont* StatFontAsset = mAssetManager->GetAssetAs<UFont>(FName("StatFont"), true);
	UFontAtlas* StatFontAtlasAsset = FObjectFactory::ConstructObject<UFontAtlas>(FName("StatFontAtlas"), *renderer, StatFontAsset, 512, 512, 2, 2);
	mAssetManager->RegisterAsset(StatFontAtlasAsset);
}      

void FEngineLoop::InitStatManager()
{
	// 매크로(SCOPE_CYCLE_COUNTER 등)가 쓰는 싱글톤과 같은 인스턴스여야 한다.
	mStatManager = new FStatManager();

	// 매크로를 처음 지날 때 자동 등록되지만, 아직 한 번도 안 지난 스탯은
	// 콘솔 목록에 뜨지 않는다. 미리 등록해 목록을 고정해둔다.

	// --- Cycle: 구간별 소요 시간(ms) ---
	mStatManager->Register(FName("Frame"), EStatType::Cycle);
	mStatManager->Register(FName("Input"), EStatType::Cycle);
	mStatManager->Register(FName("Game"), EStatType::Cycle);
	mStatManager->Register(FName("Picking"), EStatType::Cycle);
	mStatManager->Register(FName("Render"), EStatType::Cycle);
	mStatManager->Register(FName("ImGui"), EStatType::Cycle);

	// --- Counter: 프레임당 개수 ---
	mStatManager->Register(FName("DrawCalls"), EStatType::Counter);
	mStatManager->Register(FName("Triangles"), EStatType::Counter);
	mStatManager->Register(FName("Lines"), EStatType::Counter);
	mStatManager->Register(FName("Actors"), EStatType::Counter);
	mStatManager->Register(FName("Objects"), EStatType::Counter);

	// --- Memory: 현재 총량(byte). 프레임마다 리셋되지 않는다 ---
	mStatManager->Register(FName("VertexBufferMem"), EStatType::Memory);
	mStatManager->Register(FName("IndexBufferMem"), EStatType::Memory);
	mStatManager->Register(FName("TextureMem"), EStatType::Memory);
}

void FEngineLoop::Tick(bool bPumpMessages)
{
	if (GInTick) return;
	GInTick = true;

	FrameTimer->StartFrame();
	mStatManager->ResetFrame();

	// EndFrame()의 프레임 제한 대기를 빼고 재야 실제 작업 시간이 나온다.
	// 그래서 EndFrame() 앞에서 닫히는 블록으로 감싼다.
	{
	SCOPE_CYCLE_COUNTER("Frame");

	float deltaTime = FrameTimer->GetDeltaTime();
	ConsoleWindow& console = ConsoleWindow::Get();

	FRenderCollector& RenderCollector = mGraphicsManager->GetRenderCollector();
	RenderCollector.Camera = &ViewportClient->GetCamera();

	//Input Threads
	{
		SCOPE_CYCLE_COUNTER("Input");
		WindowApplication.ProcessDeferredEvents();

		//'~'누르면 콘솔 Input 포커스
		if (WindowApplication.Input.WasPressed(VK_OEM_3))
			console.RequestFocus();

		mGraphicsManager->UpdateProjectionTransition(deltaTime);
		ViewportClient->Update(deltaTime, mSceneManager, mGraphicsManager->GetPerspectiveRatio(), RenderCollector);
	}

	//Physics Threads
	{

	}

	//Game Threads
	{
		SCOPE_CYCLE_COUNTER("Game");

		if (UWorld* World = mSceneManager->GetCurrentWorld())
		{
			INC_DWORD_STAT_BY("Actors", World->GetActors().Num());
		}

		{
			// FClassInfo에 크기 정보가 없어 바이트는 못 재고 개수만 센다.
			uint32 ObjectCount = 0;
			for (const auto& Pair : FUObjectHashTables::Get().ClassToObjectListMap)
			{
				ObjectCount += Pair.second.Num();
			}
			INC_DWORD_STAT_BY("Objects", ObjectCount);
		}

		mSceneManager->Tick(deltaTime);
		mSceneManager->Update(deltaTime, RenderCollector);
	}

	//mouse picking
	{
		SCOPE_CYCLE_COUNTER("Picking");
		const FInputState& Input = WindowApplication.Input;

		// 뷰포트가 ImGui 창이 되면서 그 위에서는 io.WantCaptureMouse 가 항상 true 다.
		// 그대로 두면 씬을 클릭해도 선택이 되지 않는다. 카메라/기즈모와 같은 기준을 쓴다.
		AActor* HitActor = ViewportClient->PerformMousePicking(mGraphicsManager->GetPerspectiveRatio(), RenderCollector, *mSceneManager);
		if (mSceneManager->IsViewportHovered() && Input.WasPressed(VK_LBUTTON) && !ViewportClient->mGizmo.IsDragging() && !ViewportClient->mGizmo.IsMouseOverHandle())
		{
			if (HitActor)
			{
				mSceneManager->SetSelectedActor(HitActor);
			}
			else
			{
				mSceneManager->ResetSelectedActor();
			}
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
					// 선택된 액터의 AABB를 화면에 표시
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
						LineInfo.Color = FVector4(1.f, 0.f, 0.f, 1.f); // 빨간색
						LineInfo.Thickness = 5.0f;

						RenderCollector.LineInfos.Add(LineInfo);
					});
				}

				// 선택된 액터의 컴포넌트 시각화
				FComponentVisualizer* Visualizer = mComponentVisualizerManager->FindVisualizer(Component->GetRuntimeClass());
				if (Visualizer)
				{
					Visualizer->VisualizeComponent(Component, RenderCollector);
				}
			}
		}

		ViewportClient->mGizmo.Update(mSceneManager, mGraphicsManager->GetViewProjectionMatrix());
	}

	//Render Threads
	{
		SCOPE_CYCLE_COUNTER("Render");
		if (WindowApplication.bPendingResize)
		{
			mGraphicsManager->OnResize(WindowApplication.PendingWidth, WindowApplication.PendingHeight);
			WindowApplication.bPendingResize = false;
		}

		mGraphicsManager->Update(deltaTime);
		mGraphicsManager->Prepare(&ViewportClient->mCamera, mSceneManager->GetViewportWidth(), mSceneManager->GetViewportHeight());
		mGraphicsManager->FlushLines();
		mGraphicsManager->Render();
		
		//강조
		if (mSceneManager->GetSelectedActor())
		{
			FRenderInfo clickedRenderInfo;
			mSceneManager->GetSelectedActor()->GetFirstRenderInfo(clickedRenderInfo);
			mGraphicsManager->RenderHighLight(clickedRenderInfo);
		}

		ViewportClient->mGizmo.Render(mSceneManager, ViewportClient->mCamera.Transform.Location, mGraphicsManager->GetViewProjectionMatrix());

		//ImGui
		{
			SCOPE_CYCLE_COUNTER("ImGui");
			//ImGui Input
			mSceneManager->UpdateGUI({ *FrameTimer, mGraphicsManager, ViewportClient, mFileManager, mAssetManager });

			mGraphicsManager->GetRenderer()->BindFrameBuffer();

			ImGui::Render();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		}

		mGraphicsManager->Display();
	}

	} // SCOPE_CYCLE_COUNTER("Frame") 종료

	FrameTimer->EndFrame();

	GInTick = false;
}

void FEngineLoop::End()
{
	const std::string Value = std::format("{:.6f}", ViewportClient->GetCamera().Sensitivity);

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
	delete ViewportClient;
	delete FrameTimer;
	delete mSceneManager;
	delete mFileManager;
	delete mAssetManager;
	delete mFontManager;

	delete mGraphicsManager;
}
