#include "SceneManager.h"

#include <algorithm>
#include <format>

#include "FileManager.h"
#include "NativeFileDialog.h"
#include "EngineStatics.h"
#include "JsonUtil.h"
#include "ObjectFactory.h"
#include "PrimitiveComponent.h"
#include "TArray.h"
#include "World.h"
#include "FEditorViewportClient.h"
#include "Camera.h"
#include "Console.h"
#include "FLogManager.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/imgui_impl_dx11.h"
#include "imGui/imgui_impl_win32.h"

#include "FrameTimer.h"
#include "CubeComponent.h"
#include "UAtlasAnimationComponent.h"
#include "ActorComponent.h"
#include "WindowApplication.h"

#include "Cube.h"
#include "Assets.h"
#include "UPlaneComponent.h"
#include "USpotLightComponent.h"
#include "ASpotLight.h"
#include "UText3DComponent.h"
#include "ShowFlags.h"
#include "AStaticMeshTestActor.h"
#include "UObjectIterator.h"

#include "LaunchEngineLoop.h"
#include "StaticMesh.h"

#include "Material.h"

FSceneManager::FSceneManager(const TArray<FEditorViewportClient*>& clients, FThumbnailManager* thumbnailManager)
{
	ImGuiIO& io = ImGui::GetIO();
	mPanelWidth = io.DisplaySize.x * MIN_WIDTH_RATIO;
	mViewportX = 0;
	mViewportY = 0;
	mViewportWidth = WindowApplication.PendingWidth;
	mViewportHeight = WindowApplication.PendingHeight;

	mPropertyPanel = new FPropertyPanel();
	mPropertyPanel->Init();

	// 4개의 리프 노드(뷰포트) 생성 후 대응 클라이언트를 바로 꽂는다.
	// 순서: TopLeft=0, TopRight=1, BottomLeft=2, BottomRight=3
	SWindow* VP_TopLeft     = new SWindow(); VP_TopLeft->OwningClient     = clients[0];
	SWindow* VP_TopRight    = new SWindow(); VP_TopRight->OwningClient    = clients[1];
	SWindow* VP_BottomLeft  = new SWindow(); VP_BottomLeft->OwningClient  = clients[2];
	SWindow* VP_BottomRight = new SWindow(); VP_BottomRight->OwningClient = clients[3];

	// 4분할 루트 스플리터 생성 및 조립
	SSplitterQuad* RootSplitter = new SSplitterQuad();
	RootSplitter->TopLeft     = VP_TopLeft;
	RootSplitter->TopRight    = VP_TopRight;
	RootSplitter->BottomLeft  = VP_BottomLeft;
	RootSplitter->BottomRight = VP_BottomRight;

	// SceneManager의 루트 윈도우로 등록
	mRootWindow = RootSplitter;

	mThumbnailManager = thumbnailManager;
}

FSceneManager::~FSceneManager()
{
	delete mCurrentWorld;
}

void FSceneManager::Tick(float deltaTime)
{
	mCurrentWorld->Tick(deltaTime);
}

void FSceneManager::Update(float deltaTime, FRenderCollector& outCollector)
{
	// Todo: Save / Load
	{

	}
	mPropertyPanel->SetTarget(mSelectedActor);
	mCurrentWorld->Update(deltaTime, outCollector);
}

void FSceneManager::UpdateGUI(const FGuiReference& guiReference)
{
	//ImGui
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
#if IS_OBJ_VIEWER
	UpdateObjViewerGUI(guiReference);
#else
	{
		// Docking
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		const ImGuiID dockspaceID = ImGui::GetID("EditorDockSpace");

		const ImGuiDockNodeFlags flags = ImGuiDockNodeFlags_PassthruCentralNode;

		// 저장된 도킹 노드가 없을 때만 기본 배치 생성
		if (!ImGui::DockBuilderGetNode(dockspaceID))
		{
			ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace | flags);
			ImGui::DockBuilderSetNodeSize(dockspaceID, viewport->WorkSize);

			ImGuiID center = dockspaceID;
			ImGuiID left;
			ImGuiID bottom;

			// 왼쪽 패널 2%
			ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.2f, &left, &center);

			// 나머지 영역 아래쪽에 콘솔 30%
			ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.3f, &bottom, &center);

			ImGuiID leftTop;
			ImGuiID leftRest;
			ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.4f, &leftTop, &leftRest);

			ImGuiID leftMiddle;
			ImGuiID leftBottom;
			ImGui::DockBuilderSplitNode(leftRest, ImGuiDir_Up, 0.5f, &leftMiddle, &leftBottom);

			ImGui::DockBuilderDockWindow("Viewport", center);
			ImGui::DockBuilderDockWindow("Console Window", bottom);
			ImGui::DockBuilderDockWindow("Jungle Control Panel", leftTop);
			ImGui::DockBuilderDockWindow("Jungle Property Window", leftMiddle);
			ImGui::DockBuilderDockWindow("Outliner", leftBottom);

			ImGui::DockBuilderFinish(dockspaceID);
		}

		ImGui::DockSpaceOverViewport(dockspaceID, viewport, flags);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

		mbViewportHovered = false;
		if (ImGui::Begin("Viewport"))
		{
			const ImVec2 size = ImGui::GetContentRegionAvail();

			if (size.x > 0 && size.y > 0)
			{
				// 전체 영역의 기준점 캡처 (상대 좌표 및 절대 좌표)
				ImVec2 startCursorPos = ImGui::GetCursorPos();
				ImVec2 screenCursorPos = ImGui::GetCursorScreenPos();

				mViewportX = screenCursorPos.x;
				mViewportY = screenCursorPos.y;
				mViewportWidth = size.x;
				mViewportHeight = size.y;

				ImGui::Dummy(size);
				mbViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

				if (mRootWindow != nullptr)
				{
					SSplitterQuad* QuadSplitter = dynamic_cast<SSplitterQuad*>(mRootWindow);

					if (QuadSplitter != nullptr)
					{
						const float MouseXInViewport = static_cast<float>(WindowApplication.Input.CursorX) - mViewportX;
						const float MouseYInViewport = static_cast<float>(WindowApplication.Input.CursorY) - mViewportY;

						const float ToolbarHeight = 26.0f;

						// 각 리프 뷰포트 및 상단 툴바 렌더링
						auto ProcessLeaf = [&](SWindow* W, int32 ViewportIndex)
						{
							if (!W || !W->OwningClient) return;
							FEditorViewportClient* Client = W->OwningClient;
							const FRect rect = W->Rect;

							if (rect.GetWidth() <= 0.0f || rect.GetHeight() <= ToolbarHeight) return;

							const float RenderTop = rect.Top + ToolbarHeight;
							const float RenderHeight = rect.GetHeight() - ToolbarHeight;
							const float RenderWidth = rect.GetWidth();

							// 이번 프레임의 3D 렌더 영역 크기를 클라이언트에게 통보 (마우스 피킹과 종횡비 보정)
							Client->SetViewportArea(rect.Left, RenderTop, RenderWidth, RenderHeight);

							ImGui::PushID(Client);

							// 1. 3D 뷰포트 이미지 렌더링
							if (Client->mRenderTarget && Client->mRenderTarget->SRV)
							{
								ImGui::SetCursorPos(ImVec2(startCursorPos.x + rect.Left, startCursorPos.y + RenderTop));

								ImTextureID srv = (ImTextureID)(intptr_t)Client->mRenderTarget->SRV.Get();
								ImGui::Image(srv, ImVec2(RenderWidth, RenderHeight));

								// 뷰포트 이미지 영역 클릭 시 ActiveViewport 갱신
								if (ImGui::IsItemClicked(ImGuiMouseButton_Left) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
								{
									guiReference.ActiveViewport = Client;
								}
							}

							// 2. 상단 툴바 (Toolbar Bar) 렌더링
							{
								ImDrawList* drawList = ImGui::GetWindowDrawList();
								const ImVec2 barMin(screenCursorPos.x + rect.Left, screenCursorPos.y + rect.Top);
								const ImVec2 barMax(screenCursorPos.x + rect.Right, screenCursorPos.y + rect.Top + ToolbarHeight);

								// 툴바 배경색 및 하단 구분선
								drawList->AddRectFilled(barMin, barMax, IM_COL32(28, 28, 32, 240));
								drawList->AddLine(ImVec2(barMin.x, barMax.y), ImVec2(barMax.x, barMax.y), IM_COL32(45, 45, 50, 255));

								ImGui::SetCursorPos(ImVec2(startCursorPos.x + rect.Left + 4.0f, startCursorPos.y + rect.Top + 2.0f));

								// 뷰포트 인덱스 배지
								bool bIsActive = (Client == guiReference.ActiveViewport);
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
									guiReference.ActiveViewport = Client;
								}

								ImGui::SameLine();

								// 시점 드롭다운 (View Type)
								const char* ViewTypeNames[] = { "Perspective", "Top", "Bottom", "Left", "Right", "Front", "Back" };
								int CurrentViewType = static_cast<int>(Client->ViewportType);
								ImGui::SetNextItemWidth(110.0f);
								if (ImGui::Combo("##ViewType", &CurrentViewType, ViewTypeNames, IM_ARRAYSIZE(ViewTypeNames)))
								{
									Client->SetViewportType(static_cast<EViewportType>(CurrentViewType));
									guiReference.ActiveViewport = Client;
								}

								ImGui::SameLine();

								// 뷰 모드 드롭다운 (View Mode)
								const char* ViewModes[] = { "Lit", "Unlit", "Wireframe" };
								int CurrentViewMode = static_cast<int>(Client->ViewMode);
								ImGui::SetNextItemWidth(100.0f);
								if (ImGui::Combo("##ViewMode", &CurrentViewMode, ViewModes, IM_ARRAYSIZE(ViewModes)))
								{
									Client->ViewMode = static_cast<EViewModeIndex>(CurrentViewMode);
									guiReference.ActiveViewport = Client;
								}

								ImGui::SameLine();

								// 카메라 속도 조절
								ImGui::SetNextItemWidth(48.0f);
								if (ImGui::DragFloat("##Speed", &Client->GetCamera().Speed, 0.2f, 0.1f, 50.0f, "S:%.1f"))
								{
									guiReference.ActiveViewport = Client;
								}
								if (ImGui::IsItemHovered())
								{
									ImGui::SetTooltip("Camera Speed: %.1f", Client->GetCamera().Speed);
								}

								// 너비 여유에 따라 FOV, Sens를 인라인 또는 팝업으로 제공
								const float maxBtnWidth = 22.0f;
								const float rightButtonX = startCursorPos.x + rect.Right - maxBtnWidth - 4.0f;
								const float spaceRemaining = rightButtonX - ImGui::GetCursorPosX();

								if (spaceRemaining >= 150.0f)
								{
									ImGui::SameLine();
									ImGui::SetNextItemWidth(50.0f);
									if (ImGui::DragFloat("##FOV", &Client->GetCamera().mFovDegree, 0.5f, 5.0f, 170.0f, "FOV:%.0f"))
									{
										guiReference.ActiveViewport = Client;
									}
									if (ImGui::IsItemHovered())
									{
										ImGui::SetTooltip("Field of View (FOV: %.1f deg)", Client->GetCamera().mFovDegree);
									}

									ImGui::SameLine();
									ImGui::SetNextItemWidth(60.0f);
									if (ImGui::DragFloat("##Sens", &Client->GetCamera().Sensitivity, 0.005f, 0.01f, 1.0f, "Sens:%.2f", ImGuiSliderFlags_AlwaysClamp))
									{
										guiReference.ActiveViewport = Client;
									}
									if (ImGui::IsItemHovered())
									{
										ImGui::SetTooltip("Camera Sensitivity: %.3f", Client->GetCamera().Sensitivity);
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
										ImGui::SliderFloat("FOV", &Client->GetCamera().mFovDegree, 5.0f, 170.0f, "%.1f deg");
										ImGui::SliderFloat("Sensitivity", &Client->GetCamera().Sensitivity, 0.01f, 1.0f, "%.3f");
										ImGui::DragFloat("Speed", &Client->GetCamera().Speed, 0.2f, 0.1f, 50.0f, "%.1f");
										ImGui::EndPopup();
									}
								}

								ImGui::SameLine();

								// 포커스 버튼
								if (ImGui::Button("F"))
								{
									Client->FocusOnActor(mSelectedActor);
									guiReference.ActiveViewport = Client;
								}
								if (ImGui::IsItemHovered())
								{
									ImGui::SetTooltip("Focus on selected actor (F)");
								}

								// 최대화 토글 버튼
								if (rightButtonX > ImGui::GetCursorPosX() + 4.0f)
								{
									ImGui::SetCursorPos(ImVec2(rightButtonX, startCursorPos.y + rect.Top + 2.0f));
									const char* maxIcon = (mMaximizedViewportIndex == ViewportIndex) ? "■" : "□";
									if (ImGui::Button(maxIcon, ImVec2(maxBtnWidth, 0.0f)))
									{
										mMaximizedViewportIndex = (mMaximizedViewportIndex == ViewportIndex) ? -1 : ViewportIndex;
										guiReference.ActiveViewport = Client;
									}
									if (ImGui::IsItemHovered())
									{
										ImGui::SetTooltip(mMaximizedViewportIndex == ViewportIndex ? "Restore Viewport" : "Maximize Viewport");
									}
								}
							}

							ImGui::PopID();
						};

						SWindow* Leaves[4] = {
							QuadSplitter->TopLeft,
							QuadSplitter->TopRight,
							QuadSplitter->BottomLeft,
							QuadSplitter->BottomRight
						};

						if (mMaximizedViewportIndex >= 0 && mMaximizedViewportIndex < 4)
						{
							// 최대화 상태: 선택된 단일 뷰포트만 전체 영역 차지
							SWindow* TargetLeaf = Leaves[mMaximizedViewportIndex];
							if (TargetLeaf)
							{
								TargetLeaf->Resize({ 0.0f, 0.0f, size.x, size.y });
								ProcessLeaf(TargetLeaf, mMaximizedViewportIndex);
							}
						}
						else
						{
							// 기본 4분할 뷰
							mRootWindow->Resize({ 0.0f, 0.0f, size.x, size.y });
							for (int i = 0; i < 4; ++i)
							{
								ProcessLeaf(Leaves[i], i);
							}

							// 분할선 드래그 로직 (4분할 상태일 때만 동작)
							const FPoint LocalMousePos = { MouseXInViewport, MouseYInViewport };

							if (mbViewportHovered && QuadSplitter->DragMode == ESplitterDragMode::None)
							{
								ESplitterDragMode HoverMode = QuadSplitter->HitTestSplitter(LocalMousePos);
								if (HoverMode == ESplitterDragMode::VerticalLine)       ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
								else if (HoverMode == ESplitterDragMode::HorizontalLine) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
								else if (HoverMode == ESplitterDragMode::CenterCross)    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
							}

							if (mbViewportHovered && !ImGui::IsAnyItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
							{
								QuadSplitter->OnMouseDown(LocalMousePos);
							}

							if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
							{
								QuadSplitter->OnMouseMove(LocalMousePos);

								if (QuadSplitter->DragMode == ESplitterDragMode::VerticalLine)       ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
								else if (QuadSplitter->DragMode == ESplitterDragMode::HorizontalLine) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
								else if (QuadSplitter->DragMode == ESplitterDragMode::CenterCross)    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
							}

							if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
							{
								QuadSplitter->OnMouseUp();
							}

							// 활성화 뷰포트 테두리 렌더링
							if (guiReference.ActiveViewport != nullptr)
							{
								// 활성화된 클라이언트를 소유한 SWindow 찾기
								SWindow* ActiveWindow = nullptr;
								if (mMaximizedViewportIndex >= 0 && mMaximizedViewportIndex < 4)
								{
									ActiveWindow = Leaves[mMaximizedViewportIndex];
								}
								else
								{
									for (int i = 0; i < 4; ++i)
									{
										if (Leaves[i] && Leaves[i]->OwningClient == guiReference.ActiveViewport)
										{
											ActiveWindow = Leaves[i];
											break;
										}
									}
								}
								if (ActiveWindow != nullptr)
								{
									ImDrawList* drawList = ImGui::GetWindowDrawList();
									const FRect& r = ActiveWindow->Rect;
									// 선 두께(2px)의 절반(1px)만큼 안쪽으로 인셋하여 경계선 침범/클리핑 방지
									const float BorderThickness = 2.0f;
									const float Half = BorderThickness * 0.5f;
									const ImVec2 pMin(screenCursorPos.x + r.Left + Half, screenCursorPos.y + r.Top + Half);
									const ImVec2 pMax(screenCursorPos.x + r.Right - Half, screenCursorPos.y + r.Bottom - Half);
									// 툴바와 다른 뷰포트 이미지보다 항상 위에 주황색 테두리가 선명하게 그려짐
									drawList->AddRect(pMin, pMax, IM_COL32(255, 140, 0, 255), 0.0f, 0, BorderThickness);
								}
							}
						}
					}
				}
			}
		}
		ImGui::End();

		ImGui::PopStyleVar();
	}
	updateControlPanelGUI(guiReference);
	updatePropertyWindowGUI(guiReference);
	updateOutlinerGUI(guiReference);
	updateContentBrowserGUI(guiReference);
	ConsoleWindow::Get().Process(mPanelWidth);
#endif
}

void FSceneManager::updateControlPanelGUI(const FGuiReference& guiReference)
{
	ImGuiIO& io = ImGui::GetIO();

	float panelHeight = io.DisplaySize.y * CONTROL_PANEL_HEIGHT_RATIO;

	ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(mPanelWidth, panelHeight), ImGuiCond_FirstUseEver);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
	ImGui::Begin("Jungle Control Panel", nullptr, flags);
	mPanelWidth = ImGui::GetWindowWidth();

	/* Spawn Actor */
	// NOTE: 세 배열은 같은 순서를 유지해야 한다. 메시 이름이 비어 있으면 아래에서 따로 조립하는 타입이다.
	ImGui::SeparatorText("Spawn Actor");

	const char* ActorTypeNames[] = {
		"StaticMeshActor",
		"Sphere",
		"Cube",
		"Triangle",
		"GizmoArrow",
		"Circle",
		"SpotLight",
		"Explosion",
		"Cat"
	};

	const char* ActorMeshNames[] = {
		"",
		"SphereMesh",
		"CubeMesh",
		"TriangleMesh",
		"GizmoArrowMesh",
		"CircleMesh",
		"",
		"",
		"TestAsset"
	};

	const FClassInfo* ActorClassInfo[] = {
		AStaticMeshTestActor::GetClass(),
		UStaticMeshComponent::GetClass(),
		UStaticMeshComponent::GetClass(),
		UStaticMeshComponent::GetClass(),
		UStaticMeshComponent::GetClass(),
		UStaticMeshComponent::GetClass(),
		ASpotLight::GetClass(),
		UAtlasAnimationComponent::GetClass(),
		UStaticMeshComponent::GetClass(),
	};

	static_assert(IM_ARRAYSIZE(ActorTypeNames) == IM_ARRAYSIZE(ActorClassInfo), "ActorTypeNames and ActorClassInfo must stay the same length");
	static_assert(IM_ARRAYSIZE(ActorTypeNames) == IM_ARRAYSIZE(ActorMeshNames), "ActorTypeNames and ActorMeshNames must stay the same length");

	int32 ActorTypeIndex = mGuiInputField.ActorTypeIndex;
	int32 SpawnCount = mGuiInputField.SpawnCount;
	if (ImGui::Combo("Actor Type", &ActorTypeIndex, ActorTypeNames, IM_ARRAYSIZE(ActorTypeNames)))
	{
		mGuiInputField.ActorTypeIndex = ActorTypeIndex;
	}
	if (ImGui::Button("Spawn"))
	{
		for (int32 i = 0; i < mGuiInputField.SpawnCount; ++i)
		{
			const FClassInfo* ActorClass = ActorClassInfo[ActorTypeIndex];

			AActor* NewActor = nullptr;
			if (ActorClass->IsChildOf(UAtlasAnimationComponent::GetClass()))
			{
				NewActor = FObjectFactory::ConstructObject<AActor>();

				USpriteAtlas* ExplosionAtlas = FAssetManager::Get().GetAssetAs<USpriteAtlas>(FName("ExplosionSpriteAtlas"));

				UAtlasAnimationComponent* AnimComponent = FObjectFactory::ConstructObject<UAtlasAnimationComponent>(ExplosionAtlas);
				AnimComponent->SetRelativeLocation(FVector(0, 0, 0));
				AnimComponent->SetRelativeRotation(FRotator(0, 0, 0));
				AnimComponent->SetRelativeScale3D(FVector(1, 1, 1));
				AnimComponent->SetBillboardCamera(guiReference.ActiveViewport->GetCamera());
				AnimComponent->SetBillboard(true);
				AnimComponent->SetDepthState(true, false);
				AnimComponent->Play();

				NewActor->AddRootSceneComponent(AnimComponent);
			}
			else if (ActorClass->IsChildOf(UStaticMeshComponent::GetClass()))
			{
				NewActor = FObjectFactory::SpawnPrimitiveActor(FName(ActorMeshNames[ActorTypeIndex]), FVector(0, 0, 0), FRotator(0, 0, 0), FVector(1, 1, 1));
			}
			else if (ActorClass->IsChildOf(ASpotLight::GetClass()))
			{
				NewActor = FObjectFactory::ConstructObject<ASpotLight>();

				UTexture2D* SpotLightTexture = FAssetManager::Get().GetAssetAs<UTexture2D>(FName("SpotLightIcon"), true);

				UPlaneComponent* PlaneComponent = FObjectFactory::ConstructObject<UPlaneComponent>(FVector(0, 0, 0), FRotator(0, 0, 0), FVector(1, 1, 1));
				PlaneComponent->SetBillboardCamera(guiReference.ActiveViewport->GetCamera());
				PlaneComponent->SetBillboard(true);
				PlaneComponent->SetTexture(SpotLightTexture);
				PlaneComponent->SetBlendState(ERenderBlendMode::Transparent);
				PlaneComponent->SetDepthState(true, false);

				NewActor->AddComponent(PlaneComponent);
			}
			else if (ActorClass->IsChildOf(AStaticMeshTestActor::GetClass()))
			{
				NewActor = FObjectFactory::ConstructObject<AStaticMeshTestActor>();
			}
			else
			{
				UE_LOG_ERROR("Unknown actor class: %s", ActorClass->Name.CStr());
			}

			if (NewActor)
			{
				UText3DComponent* Text3DComponent = FObjectFactory::ConstructObject<UText3DComponent>(FVector(0, 0, 1), FRotator(0, 0, 0), FVector(1, 1, 1));
				Text3DComponent->SetBillboardCamera(guiReference.ActiveViewport->GetCamera());
				Text3DComponent->SetBillboard(true);
				Text3DComponent->SetText(Utf2Wide(std::format("UUID: {}", NewActor->UUID)));
				Text3DComponent->SetFontAtlasAsset(FAssetManager::Get().GetAssetAs<UFontAtlas>(FName("TestFontAtlas")));
				Text3DComponent->SetDepthState(false, false);
				
				NewActor->AddComponent(Text3DComponent);

				mCurrentWorld->AddActor(NewActor);
			}
		}
	}
	ImGui::SameLine();
	if (ImGui::InputInt("Number of spawn", &SpawnCount))
	{
		if (SpawnCount < 1)
		{
			SpawnCount = 1;
		}
		mGuiInputField.SpawnCount = SpawnCount;
	}

	/*Scene Control*/
	ImGui::SeparatorText("Scene Control");

	const std::filesystem::path sceneDirectory = std::filesystem::absolute(std::filesystem::path(kDefaultAssetsPath) / std::filesystem::path(kSceneDataDir));

	void* ownerWindow = ImGui::GetMainViewport()->PlatformHandleRaw;

	if (ImGui::Button("New scene"))
	{
		guiReference.ActiveViewport->Reset();
		NewScene();
	}

	ImGui::SameLine();

	if (ImGui::Button("Save scene"))
	{
		try
		{
			// 저장 대화상자의 초기 폴더가 반드시 존재하도록 한다.
			//std::filesystem::create_directories(sceneDirectory);

			const std::optional<std::filesystem::path> selectedPath =
				FNativeFileDialog::SaveScene(
					ownerWindow,
					sceneDirectory);

			// 취소 버튼을 누른 경우에는 아무 작업도 하지 않는다.
			if (selectedPath.has_value())
			{
				SaveScene(selectedPath.value(), *guiReference.FileManager);

				UE_LOG("Scene saved: %s", selectedPath->string().c_str());
			}
		}
		catch (const std::exception& e)
		{
			UE_LOG_ERROR(
				"Failed to save scene: %s",
				e.what());
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Load scene"))
	{
		try
		{
			//std::filesystem::create_directories(sceneDirectory);

			const std::optional<std::filesystem::path> selectedPath =
				FNativeFileDialog::OpenScene(
					ownerWindow,
					sceneDirectory);

			// 취소한 경우에는 현재 씬과 카메라 상태를 건드리지 않는다.
			if (selectedPath.has_value())
			{
				LoadScene(
					selectedPath.value(),
					*guiReference.FileManager);

				// 파일 로드가 실행된 뒤에만 카메라를 초기화한다.
				guiReference.ActiveViewport->Reset();

				// 여기부터 런타임 카메라 재연결
				FCamera& Camera =
					guiReference.ActiveViewport->GetCamera();

				for (AActor* Actor : mCurrentWorld->GetActors())
				{
					if (ASpotLight* SpotLight =
						Actor->Cast<ASpotLight>())
					{
						SpotLight->RestoreRuntimeCamera(Camera);
					}

					for (UActorComponent* Component :
						Actor->GetComponents())
					{
						if (UAtlasAnimationComponent* Atlas = Component->Cast<UAtlasAnimationComponent>())
						{
							Atlas->RestoreRuntimeCamera(Camera);
						}


						if (UText3DComponent* Text = Component->Cast<UText3DComponent>())
						{
							Text->RestoreRuntimeResources(Camera);

							// 기존 씬 파일에는 mText가 저장되지 않았으므로
							// 빈 텍스트라면 UUID 문구를 재생성한다.
							if (Text->GetText().empty())
							{
								Text->SetText(
									Utf2Wide(
										FString(
											std::format("UUID: {}", Actor->UUID)
										)
									)
								);
							}
						}
					}

					


				}

				UE_LOG(
					"Scene loaded: %s",
					selectedPath->string().c_str());
			}
		}
		catch (const std::exception& e)
		{
			UE_LOG_ERROR(
				"Failed to load scene: %s",
				e.what());
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Import"))
	{
		const std::optional<std::filesystem::path> selectedPath = FNativeFileDialog::OpenObjFile(
			ownerWindow,
			sceneDirectory
		);

		if (selectedPath.has_value())
		{
			const std::filesystem::path& Path = selectedPath.value();
			FFileManager& FileManager = const_cast<FFileManager&>(*guiReference.FileManager);
			URenderer* Renderer = guiReference.GraphicsManager->GetRenderer();
			FAssetManager* AssetManager = guiReference.AssetManager;

			// Import()가 obj/mtl을 파싱해서 .uasset으로 굽고 등록까지 한다.
			// 같은 원본을 다시 고르면 재임포트로 판단해 같은 키에 다시 굽는다.
			FStaticMeshAssetLoader* Loader = AssetManager->GetOrCreateLoader<FStaticMeshAssetLoader>(*Renderer, *AssetManager);
			FName AssetName = Loader->Import(Path, FileManager);

			AssetManager->LoadAsset(AssetName, true);
		}
	}

	/* Camera Control */
	ImGui::SeparatorText("Camera Control");

	FCamera& camera = guiReference.ActiveViewport->GetCamera();

	if (ImGui::BeginCombo("##ShowFlags", "Show Flags"))
	{
		FShowFlags& showFlags = FShowFlags::Get();
		for (const FShowFlagInfo& flagInfo : GShowFlagInfos)
		{
			bool bEnabled = showFlags.IsEnabled(flagInfo.Flag);
			if (ImGui::Checkbox(flagInfo.Name, &bEnabled))
			{
				showFlags.SetEnabled(flagInfo.Flag, bEnabled);
			}
		}

		bool bOrthographic = guiReference.GraphicsManager->IsOrthographicTarget();
		if (ImGui::Checkbox("Orthogonal", &bOrthographic))
		{
			// Preserve the camera and ortho zoom; animate only the projection ratio.
			guiReference.GraphicsManager->StartProjectionTransition(bOrthographic);
		}

		ImGui::EndCombo();
	}

	{
		static constexpr int32 GridGapValues[] = { 1, 5, 10, 50, 100, 500 };
		static constexpr const char* GridGapLabels[] = { "(1)", "(5)", "(10)", "(50)", "(100)", "(500)" };
		constexpr int StepCount = IM_ARRAYSIZE(GridGapValues);
		const int32 GridGap = guiReference.GraphicsManager->GetGridGap();
		int SelectedIndex = 0;
		for (int i = 1; i < StepCount; ++i)
		{
			if (GridGap >= (GridGapValues[i - 1] + GridGapValues[i]) / 2.0f)
			{
				SelectedIndex = i;
			}
		}

		ImGui::Text("Grid Gap: %d", GridGap);
		const ImGuiStyle& Style = ImGui::GetStyle();
		const float FontSize = ImGui::GetFontSize();
		const float LabelWidth = ImGui::CalcTextSize("(500)").x;
		const float Width = (std::max)(ImGui::GetContentRegionAvail().x,
			(LabelWidth + Style.ItemInnerSpacing.x) * StepCount);
		const float Padding = LabelWidth * 0.5f;
		const ImVec2 Origin = ImGui::GetCursorScreenPos();
		const float TrackLeft = Origin.x + Padding;
		const float TrackWidth = Width - Padding * 2.0f;
		const float TrackY = Origin.y + FontSize;
		const float TrackHeight = FontSize * 0.3f;
		const float LabelY = TrackY + TrackHeight + Style.ItemInnerSpacing.y;
		ImGui::InvisibleButton("##GridGapSelector",
			ImVec2(Width, LabelY + FontSize - Origin.y));
		const bool bActive = ImGui::IsItemActive();
		const bool bHovered = ImGui::IsItemHovered();
		bool bChanged = false;
		if (bActive && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			// Snap to the closest displayed step, including when dragging past either end.
			const float Position = std::clamp((io.MousePos.x - TrackLeft) / TrackWidth, 0.0f, 1.0f);
			SelectedIndex = static_cast<int>(Position * (StepCount - 1) + 0.5f);
			bChanged = true;
		}
		if (bChanged && GridGapValues[SelectedIndex] != GridGap)
		{
			guiReference.GraphicsManager->SetGridGap(GridGapValues[SelectedIndex]);
		}

		if (ImGui::IsItemVisible())
		{
			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			const ImU32 TrackColor = ImGui::GetColorU32(bActive ? ImGuiCol_FrameBgActive :
				(bHovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg));
			const ImU32 HandleColor = ImGui::GetColorU32(bActive ? ImGuiCol_SliderGrabActive : ImGuiCol_SliderGrab);
			DrawList->AddRectFilled(ImVec2(TrackLeft, TrackY),
				ImVec2(TrackLeft + TrackWidth, TrackY + TrackHeight), TrackColor, Style.FrameRounding);
			for (int i = 0; i < StepCount; ++i)
			{
				const float X = TrackLeft + TrackWidth * i / (StepCount - 1);
				const ImU32 LabelColor = ImGui::GetColorU32(i == SelectedIndex ? ImGuiCol_Text : ImGuiCol_TextDisabled);
				DrawList->AddLine(ImVec2(X, TrackY), ImVec2(X, TrackY + TrackHeight), LabelColor);
				DrawList->AddText(ImVec2(X - ImGui::CalcTextSize(GridGapLabels[i]).x * 0.5f, LabelY),
					LabelColor, GridGapLabels[i]);
			}
			const float HandleX = TrackLeft + TrackWidth * SelectedIndex / (StepCount - 1);
			DrawList->AddTriangleFilled(ImVec2(HandleX - FontSize * 0.4f, Origin.y),
				ImVec2(HandleX + FontSize * 0.4f, Origin.y), ImVec2(HandleX, TrackY + TrackHeight), HandleColor);
		}
	}

	// 1) 라벨 텍스트를 먼저 그리고 같은 줄로
	ImGui::Text("Location");
	ImGui::SameLine();

	// 2) 텍스트를 그린 "뒤"의 남은 폭을 기준으로 계산
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float itemWidth = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;

	ImGui::SetNextItemWidth(itemWidth);
	ImGui::DragFloat("##CamLocX", &camera.Transform.Location.x, 0.1f, 10.0f);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(itemWidth);
	ImGui::DragFloat("##CamLocY", &camera.Transform.Location.y, 0.1f, 10.0f);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(itemWidth);
	ImGui::DragFloat("##CamLocZ", &camera.Transform.Location.z, 0.1f, 10.0f);

	ImGui::Text("Rotation");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(itemWidth);
	ImGui::DragFloat("##CamRotX", &camera.Transform.Rotation.Roll, 0.1f, 180.0f);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(itemWidth);
	ImGui::DragFloat("##CamRotY", &camera.Transform.Rotation.Pitch, 0.1f, 180.0f);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(itemWidth);
	ImGui::DragFloat("##CamRotZ", &camera.Transform.Rotation.Yaw, 0.1f, 180.0f);

	ImGui::End();
}

void FSceneManager::updatePropertyWindowGUI(const FGuiReference& guiReference)
{
	ImGuiIO& io = ImGui::GetIO();

	float controlPanelHeight = io.DisplaySize.y * CONTROL_PANEL_HEIGHT_RATIO;
	float propertyHeight = io.DisplaySize.y * WINDOW_PROPERTY_HEIGHT_RATIO;

	ImGui::SetNextWindowPos(ImVec2(0.0f, controlPanelHeight), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(mPanelWidth, propertyHeight), ImGuiCond_FirstUseEver);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

	ImGui::Begin("Jungle Property Window", nullptr, flags);

	mPanelWidth = ImGui::GetWindowWidth();
	mPropertyPanel->OnRender();
	ImGui::End();
}


void FSceneManager::updateOutlinerGUI(const FGuiReference& guiReference)
{
	ImGuiIO& io = ImGui::GetIO();

	float offsetHeight = io.DisplaySize.y * (CONTROL_PANEL_HEIGHT_RATIO + WINDOW_PROPERTY_HEIGHT_RATIO);
	float objectListPanelHeight = io.DisplaySize.y - offsetHeight;

	ImGui::SetNextWindowPos(ImVec2(0.0f, offsetHeight), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(mPanelWidth, objectListPanelHeight), ImGuiCond_FirstUseEver);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;

	ImGui::Begin("Outliner", nullptr, flags);
	{
		if (ImGui::BeginChild("ObjectList", ImVec2(0, 0),
			ImGuiChildFlags_Borders))
		{
			// 월드에 스폰된 액터를 나열한다.
			if (mGuiInputField.LastGUObjectRevision != UObject::GetGObjectRevision())
			{
				mGuiInputField.SortedActorLists.Empty();

				if (mCurrentWorld != nullptr)
				{
					mGuiInputField.SortedActorLists = mCurrentWorld->GetActors();

					// Sort the actors by UUID
					std::sort(mGuiInputField.SortedActorLists.begin(), mGuiInputField.SortedActorLists.end(),
						[](AActor* a, AActor* b) { return a->UUID < b->UUID; });
				}

				mGuiInputField.LastGUObjectRevision = UObject::GetGObjectRevision();
			}

			int32 selectedActorUUID = mSelectedActor
				? mSelectedActor->UUID
				: -1;

			// Todo: rbegin()
			//for (AActor* actor : mGuiInputField.SortedActorLists)

			AActor* bDeleteActorOrNull = nullptr;
			for (unsigned int actorsIndex = 0; actorsIndex < mGuiInputField.SortedActorLists.Num(); ++actorsIndex)
			{
				AActor* actor = mGuiInputField.SortedActorLists[actorsIndex];

				bool bSelected = false;
				ImGui::PushID(actor->UUID); // Ensure unique ID for each child

				// Highlight the frame if this object is the clicked actor
				if (actor->UUID == selectedActorUUID)
				{
					bSelected = true;
					ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(255, 255, 0, 50)); // Light yellow background
				}

				if (ImGui::BeginChild("ObjectFrame", ImVec2(0, 0), ImGuiChildFlags_FrameStyle | ImGuiChildFlags_AutoResizeY))
				{
					ImGui::Text("Class: %s", actor->GetRuntimeClass()->Name.CStr());
					ImGui::Text("UUID: %d", actor->UUID);

					// TODO: Move implement delete to where?
					if (ImGui::Button("Select"))
					{
						SetSelectedActor(actor);
					}
					else
					{
						ImGui::SameLine();
						if (ImGui::Button("Delete"))
						{
							bDeleteActorOrNull = actor;
						}
					}
				}
				ImGui::EndChild();

				if (bSelected)
				{
					ImGui::PopStyleColor(); // Pop the border color if it was pushed
				}


				ImGui::PopID();
			}

			if (bDeleteActorOrNull != nullptr)
			{
				AActor* deleteActor = bDeleteActorOrNull;

				if (mSelectedActor != nullptr && mSelectedActor->UUID == deleteActor->UUID)
				{
					mSelectedActor = nullptr;
				}

				assert(mCurrentWorld != nullptr);
				mCurrentWorld->RemoveActor(deleteActor->UUID);

				delete deleteActor;
			}
		}
		ImGui::EndChild();
	}
	ImGui::End();
}


void FSceneManager::NewScene()
{
	mPropertyPanel->SetTarget(nullptr);

	if (mCurrentWorld != nullptr)
	{
		delete mCurrentWorld;
	}

	UEngineStatics::SetNextUUID(0);
	ResetSelectedActor();
	mCurrentWorld = FObjectFactory::ConstructObject<UWorld>();
	mPropertyPanel->SetWorld(mCurrentWorld);
}

void FSceneManager::DeleteScene()
{
	if (mCurrentWorld != nullptr)
	{
		delete mCurrentWorld;
		mCurrentWorld = nullptr;
	}
	ResetSelectedActor();
}

void FSceneManager::SaveScene(
	const std::filesystem::path& scenePath,
	const FFileManager& fileManager)
{
	if (mCurrentWorld == nullptr)
	{
		throw std::runtime_error(
			"Cannot save scene because current world is null.");
	}

	uint32 version = 0;

	// 기존 파일이 있으면 Version을 유지한다.
	try
	{
		const FString previousSceneString =
			fileManager.ReadFileToString(scenePath);

		const json::JSON previousSceneJson =
			json::JSON::Load(previousSceneString);

		if (previousSceneJson.hasKey("Version") &&
			previousSceneJson.at("Version").JSONType() ==
			json::JSON::Class::Integral)
		{
			version =
				previousSceneJson.at("Version").ToInt();
		}
	}
	catch (const std::exception&)
	{
		// 새로 저장하는 파일이면 Version 0부터 시작한다.
		version = 0;
	}

	json::JSON sceneJson =
		json::JSON::Make(json::JSON::Class::Object);

	json::JSON worldJson =
		json::JSON::Make(json::JSON::Class::Object);

	mCurrentWorld->SerializeClass(worldJson);

	sceneJson["Version"] = version;
	sceneJson["NextUUID"] = UEngineStatics::GetNextUUID();
	sceneJson["World"] = worldJson;

	const FString jsonString(
		sceneJson.dump(1, "  "));

	fileManager.WriteStringToFile(
		scenePath,
		jsonString);
}

void FSceneManager::LoadScene(
	const std::filesystem::path& scenePath,
	const FFileManager& fileManager)
{
	const FString jsonString =
		fileManager.ReadFileToString(scenePath);

	const json::JSON sceneJson =
		json::JSON::Load(jsonString);

	if (!sceneJson.hasKey("NextUUID") ||
		sceneJson.at("NextUUID").JSONType() !=
		json::JSON::Class::Integral)
	{
		throw std::runtime_error(
			std::format(
				"Scene file '{}' does not contain valid NextUUID data.",
				scenePath.string()));
	}

	if (!sceneJson.hasKey("World") ||
		sceneJson.at("World").JSONType() !=
		json::JSON::Class::Object)
	{
		throw std::runtime_error(
			std::format(
				"Scene file '{}' does not contain valid World data.",
				scenePath.string()));
	}

	const uint32 nextUUID =
		sceneJson.at("NextUUID").ToInt();

	const json::JSON worldJson =
		sceneJson.at("World");

	UWorld* newWorld =
		FObjectFactory::LoadObject<UWorld>(worldJson);

	if (newWorld == nullptr)
	{
		throw std::runtime_error(
			std::format(
				"Failed to deserialize world from '{}'.",
				scenePath.string()));
	}

	// 새 월드 생성이 성공한 경우에만 기존 월드를 교체한다.
	mPropertyPanel->SetTarget(nullptr);
	delete mCurrentWorld;
	mCurrentWorld = newWorld;
	mPropertyPanel->SetWorld(mCurrentWorld);
	UEngineStatics::SetNextUUID(nextUUID);
	ResetSelectedActor();
}

void  FSceneManager::SetSelectedActor(AActor* actor)
{
	if (actor == nullptr)
	{
		UE_LOG_WARN("SetSelectedActor: Attempted to set selected actor to nullptr.");
		return;
	}

	if (actor == mSelectedActor)
	{
		UE_LOG_WARN("SetSelectedActor: Actor with UUID %d is already selected.", actor->UUID);
		return; // No change
	}

	UE_LOG_WARN("SetSelectedActor: Actor with UUID %d is now selected.", actor->UUID);
	mSelectedActor = actor;
}

float FSceneManager::GetPanelWidth() const
{
	return mPanelWidth;
}

void FSceneManager::InitObjViewer(const char* CmdLine)
{
	NewScene();
	UE_LOG("OBJ Viewer Path: %s", CmdLine);
}

void FSceneManager::UpdateObjViewerGUI(const FGuiReference& guiReference)
{
	if (guiReference.ActiveViewport == nullptr)	return;
	//if (guiReference.ActiveViewport->IsEmpty())	return;

	FEditorViewportClient* ViewportClient = (guiReference.ActiveViewport);
	if (ViewportClient == nullptr) return;
	
	const ImGuiViewport* MainViewport = ImGui::GetMainViewport();

	ImGui::SetNextWindowPos(MainViewport->Pos, ImGuiCond_Always);

	ImGui::SetNextWindowSize(MainViewport->Size, ImGuiCond_Always);

	const ImGuiWindowFlags WindowFlags =ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
											ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

	ImGui::Begin("OBJ Viewer", nullptr, WindowFlags);

	if (ImGui::Button("Open OBJ")) {
		const std::filesystem::path sceneDirectory = std::filesystem::absolute(std::filesystem::path(kDefaultAssetsPath) / std::filesystem::path(kSceneDataDir));
		void* ownerWindow = ImGui::GetMainViewport()->PlatformHandleRaw;

		const std::optional<std::filesystem::path> selectedPath = FNativeFileDialog::OpenObjFile(ownerWindow, sceneDirectory);

		if (selectedPath.has_value()) {
			const std::filesystem::path& Path = selectedPath.value();
			FFileManager& FileManager = const_cast<FFileManager&>(*guiReference.FileManager);
			URenderer* Renderer = guiReference.GraphicsManager->GetRenderer();
			FAssetManager* AssetManager = guiReference.AssetManager;

			// Import()가 obj/mtl을 파싱해서 .uasset으로 굽고 등록까지 한다.
			// 같은 원본을 다시 고르면 재임포트로 판단해 같은 키에 다시 굽는다.
			FStaticMeshAssetLoader* Loader = AssetManager->GetOrCreateLoader<FStaticMeshAssetLoader>(*Renderer, *AssetManager);
			FName AssetName = Loader->Import(Path, FileManager);

			AssetManager->LoadAsset(AssetName, true);

			NewScene();

			UStaticMesh* MeshAsset = guiReference.AssetManager->GetAssetAs<UStaticMesh>(AssetName, true);

			if (MeshAsset != nullptr) {
				const FAABB& Bounds = MeshAsset->GetLocalBoundingBox();
				const FVector Center = (Bounds.Min + Bounds.Max) * 0.5f;
				AActor* NewActor = FObjectFactory::SpawnPrimitiveActor(AssetName, -Center, FRotator(0.0f, 0.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f));
				if (NewActor != nullptr) {
					mCurrentWorld->AddActor(NewActor);
					ViewportClient->SetViewerActor(NewActor);
					ViewportClient->FocusOnViewerActor();
					ViewportClient->Reset();
				}
			}

		}
	}

	ImGui::Separator();
	ImVec2 ContentSize = ImGui::GetContentRegionAvail();
	float PropertyPanelWidth = 350.0f;
	float ViewportWidth = ContentSize.x - PropertyPanelWidth - 8.0f;

	if (ImGui::BeginChild("ViewportRegion", ImVec2(ViewportWidth, ContentSize.y), false)) {
		const ImVec2 Size = ImGui::GetContentRegionAvail();

		if (Size.x > 0.0f && Size.y > 0.0f) {
			ImVec2 ScreenPos = ImGui::GetCursorScreenPos();

			mViewportX = ScreenPos.x;
			mViewportY = ScreenPos.y;
			mViewportWidth = Size.x;
			mViewportHeight = Size.y;

			ViewportClient->SetViewportArea(0.0f, 0.0f, Size.x, Size.y);

			guiReference.ActiveViewport = ViewportClient;

			if (ViewportClient->mRenderTarget && ViewportClient->mRenderTarget->SRV) {
				ImTextureID SRV = (ImTextureID)(intptr_t)ViewportClient->mRenderTarget->SRV.Get();

				ImGui::Image(SRV, Size);

				mbViewportHovered = ImGui::IsWindowHovered();
			}
			else {
				mbViewportHovered = false;
			}
		}
	}
	ImGui::EndChild();
	ImGui::SameLine();

	if (ImGui::BeginChild("PropertiesRegion", ImVec2(PropertyPanelWidth, ContentSize.y), true)) {
		ImGui::Text("Details");
		ImGui::Separator();

		if (mPropertyPanel != nullptr) {
			mPropertyPanel->OnPropertyChanged = [ViewportClient]() {
				ViewportClient->FocusOnViewerActor();
			};
			mPropertyPanel->SetTarget(ViewportClient->mViewerActor);
			mPropertyPanel->OnRender();
		}
		else {
			ImGui::TextDisabled("PropertyPanel is uninitialized.");
		}
	}
	ImGui::EndChild();
	
	ImGui::End();
	ImGui::PopStyleVar();
}

const TArray<FRenderInfo> FSceneManager::GetAxisRenderInfos()
{
	// TODO: Implement axis render info retrieval logic
	return TArray<FRenderInfo>();
}

void FSceneManager::updateContentBrowserGUI(const FGuiReference& guiReference)
{
	ImGuiIO& io = ImGui::GetIO();

	if (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) && ImGui::IsKeyPressed(ImGuiKey_Space)) {
		mShowContentBrowser = !mShowContentBrowser;
	}

	if (!mShowContentBrowser)	return;

	ImGuiViewport* viewport = ImGui::GetMainViewport();

	const float browserHeight = io.DisplaySize.y * WINDOW_PROPERTY_HEIGHT_RATIO;

	// 화면의 가장 아래에 고정
	ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - browserHeight), ImGuiCond_Always);

	ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, browserHeight), ImGuiCond_Always);

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking;


	if (!ImGui::Begin("Jungle Content Browser", nullptr, flags)) {
		ImGui::End();
		return;
	}

	//상단 툴바
	if (mCurrentDirectory != mRootPath) {
		if (ImGui::Button("<- Back")) {
			mCurrentDirectory = mCurrentDirectory.parent_path();
		}
		ImGui::SameLine();
	}
	ImGui::Text("Path: %s", mCurrentDirectory.string().c_str());

	//우측 상단 검색창
	ImGui::SameLine(ImGui::GetContentRegionAvail().x - 200.0f);
	ImGui::SetNextItemWidth(200.0f);
	ImGui::InputTextWithHint("##SearchAsset", "Search...", mSearchBuffer, IM_ARRAYSIZE(mSearchBuffer));

	ImGui::Separator();

	//2열 영역 분할
	if (ImGui::BeginTable("ContentBrowserLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
		ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 180.0f);
		ImGui::TableSetupColumn("Assets", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		ImGui::BeginChild("FolderTreeChildArea", ImVec2(0, 0), false);
		drawFolderTree(mRootPath);
		ImGui::EndChild();

		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("AssetGridChildArea", ImVec2(0, 0), false);
		drawAssetGrid();
		ImGui::EndChild();

		ImGui::EndTable();
	}
	ImGui::End();
}

void FSceneManager::drawFolderTree(const std::filesystem::path& currentPath)
{
	if (!std::filesystem::exists(currentPath)) return;

	for (const auto& entry : std::filesystem::directory_iterator(currentPath)) {
		if (!entry.is_directory()) continue;

		const auto& path = entry.path();
		std::string folderName = path.filename().string();

		ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

		if (mCurrentDirectory == path) {
			nodeFlags |= ImGuiTreeNodeFlags_Selected;
		}

		bool hasSubFolders = false;
		for (const auto& subEntry : std::filesystem::directory_iterator(path)) {
			if (subEntry.is_directory()) {
				hasSubFolders = true;
				break;
			}
		}

		if (!hasSubFolders) {
			nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		bool bNodeOpen = ImGui::TreeNodeEx(path.string().c_str(), nodeFlags, "%s", folderName.c_str());

		if (ImGui::IsItemClicked())
		{
			mCurrentDirectory = path; // 클릭 시 우측 그리드 경로 변경
		}

		if (bNodeOpen && hasSubFolders)
		{
			drawFolderTree(path); // 재귀 호출
			ImGui::TreePop();
		}
	}
}

// ----------------------------------------------------
// 우측: 반응형 에셋 타일 그리드 & Drag & Drop
// ----------------------------------------------------
void FSceneManager::drawAssetGrid() {
	if (!std::filesystem::exists(mCurrentDirectory)) return;

	float padding = 16.0f;
	float cellSize = mThumbnailSize + padding;
	float panelWidth = ImGui::GetContentRegionAvail().x;
	int columnCount = static_cast<int>(panelWidth / cellSize);
	if (columnCount < 1) columnCount = 1;

	ImGui::Columns(columnCount, 0, false);

	for (const auto& entry : std::filesystem::directory_iterator(mCurrentDirectory))
	{
		const auto& path = entry.path();
		std::string filename = path.filename().string();
		bool isDirectory = entry.is_directory();

		if (strlen(mSearchBuffer) > 0 && filename.find(mSearchBuffer) == std::string::npos)
			continue;

		ImGui::PushID(filename.c_str());

		// [핵심 연결] ThumbnailManager로부터 ImTextureID(ID3D11ShaderResourceView*) 획득
		ImTextureID thumbID = mThumbnailManager->GetThumbnail(path, isDirectory);

		ImVec4 bgColor = (mSelectedAssetPath == path) ? ImVec4(0.2f, 0.6f, 1.0f, 0.6f) : ImVec4(0, 0, 0, 0);

		if (mSelectedAssetPath == path)
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 0.6f));

		// 글자 버튼 대신 이미지 버튼으로 렌더링
		bool clicked = ImGui::ImageButton(
			filename.c_str(),
			thumbID,
			ImVec2(mThumbnailSize, mThumbnailSize),
			ImVec2(0, 0), ImVec2(1, 1),
			bgColor,
			ImVec4(1, 1, 1, 1)
		);

		if (mSelectedAssetPath == path)
			ImGui::PopStyleColor();

		if (clicked || ImGui::IsItemClicked(ImGuiMouseButton_Left))
			mSelectedAssetPath = path;

		if (isDirectory && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			mCurrentDirectory /= path.filename();

		if (!isDirectory && ImGui::BeginDragDropSource())
		{
			std::string pathString = path.string();
			ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", pathString.c_str(), pathString.size() + 1);
			ImGui::Text("Dragging: %s", filename.c_str());
			ImGui::EndDragDropSource();
		}

		ImGui::TextWrapped("%s", filename.c_str());
		ImGui::NextColumn();
		ImGui::PopID();
	}
	ImGui::Columns(1);
}


//
//FSceneData FSceneManager::ReadSceneData(
//	std::string_view sceneName,
//	const FFileManager& fileManager)
//{
//	FString fileName = sceneName;
//	fileName += kSceneDataSuffix;
//
//	json::JSON jsonData = json::JSON::Load(fileManager.ReadFileToString(fileName));
//	FSceneData sceneData = FSceneData(jsonData);
//	return sceneData;
//}
//
//UWorld* FSceneManager::BuildWorldFromSceneData(const FSceneData& sceneData)
//{
//	//UWorld* newWorld = FObjectFactory::ConstructObject<UWorld>();
//
//	//for (const auto& [UUID, primitiveData] : sceneData.Primitives) 
//	//{
//	//	// TODO: Replace AActor creation logic later
//	//	AActor* newActor = FObjectFactory::ConstructObject<AActor>();
//	//	UPrimitiveComponent* newPrimitiveComponent =
//	//		FObjectFactory::ConstructObject<UPrimitiveComponent>(
//	//			);
//	//}
//
//	//UEngineStatics::SetNextUUID(sceneData.NextUUID);
//	throw std::logic_error("BuildWorldFromSceneData is not implemented yet.");
//	return nullptr;
//}
