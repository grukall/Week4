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

FSceneManager::FSceneManager()
{
	ImGuiIO& io = ImGui::GetIO();
	mPanelWidth = io.DisplaySize.x * MIN_WIDTH_RATIO;
	mViewportX = 0;
	mViewportY = 0;
	mViewportWidth = WindowApplication.PendingWidth;
	mViewportHeight = WindowApplication.PendingHeight;

	mPropertyPanel = new FPropertyPanel();
	mPropertyPanel->Init();
	//mCurrentWorld = FObjectFactory::ConstructObject<UWorld>();

	// Todo: Test code, move to other function
	//{
	//	UCubeComponent* cubeComponent = FObjectFactory::ConstructObject<UCubeComponent>(FVector(0), FRotator(), FVector(1));
	//	AActor* cubeActor = FObjectFactory::ConstructObject<AActor>();
	//	cubeActor->AddComponent(cubeComponent);
	//	mCurrentWorld->AddActor(cubeActor);

	//	UCubeComponent* cubeComponent2 = FObjectFactory::ConstructObject<UCubeComponent>(FVector(1, 1, 1), FRotator(), FVector(0.5));
	//	AActor* cubeActor2 = FObjectFactory::ConstructObject<AActor>();
	//	cubeActor2->AddComponent(cubeComponent2);
	//	mCurrentWorld->AddActor(cubeActor2);
	//}
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
				const TSharedPtr<FRenderTarget2D>& sceneRenderTarget = guiReference.GraphicsManager->GetSceneRenderTarget();
				ImGui::Image((ImTextureID)(intptr_t)sceneRenderTarget->SRV.Get(), size);
				mbViewportHovered = ImGui::IsItemHovered();

				const ImVec2 imageMin = ImGui::GetItemRectMin();
				const ImVec2 imageMax = ImGui::GetItemRectMax();

				mViewportX = imageMin.x;
				mViewportY = imageMin.y;
				mViewportWidth = size.x;
				mViewportHeight = size.y;
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
				AnimComponent->SetBillboardCamera(guiReference.ViewportClient->GetCamera());
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
				PlaneComponent->SetBillboardCamera(guiReference.ViewportClient->GetCamera());
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
				Text3DComponent->SetBillboardCamera(guiReference.ViewportClient->GetCamera());
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
		guiReference.ViewportClient->Reset();
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
				guiReference.ViewportClient->Reset();

				// 여기부터 런타임 카메라 재연결
				FCamera& Camera =
					guiReference.ViewportClient->GetCamera();

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
			FFileAssetSource* FileAssetSource = new FFileAssetSource(FileManager, Path);
			FName AssetName = FName(FString(FileManager.MakeRelativeToRoot(Path).string()));

			AssetManager->RegisterAsset<FStaticMeshAssetLoader>(AssetName, FileAssetSource, *Renderer, *AssetManager);
			AssetManager->LoadAsset(AssetName, true);
		}
	}

	/* Camera Control */
	ImGui::SeparatorText("Camera Control");

	FCamera& camera = guiReference.ViewportClient->GetCamera();
	URenderer* renderer = guiReference.GraphicsManager->GetRenderer();

	const char* viewModeNames[] = { "Lit", "Unlit", "Wireframe" };

	EViewModeIndex currentViewMode = guiReference.GraphicsManager->GetViewModeIndex();
	int32 currentViewModeIndex = static_cast<int32>(currentViewMode);
	// Combo는 선택이 바뀐 프레임에만 true를 돌려주고, 바뀐 값은 이미
	// currentViewModeIndex에 들어 있다. 그 안에서 Checkbox를 그리면
	// 한 프레임만 나타났다 사라져 클릭할 수 없다.
	if (ImGui::Combo("View Mode", &currentViewModeIndex, viewModeNames, IM_ARRAYSIZE(viewModeNames)))
	{
		guiReference.GraphicsManager->SetViewModeIndex(static_cast<EViewModeIndex>(currentViewModeIndex));
	}
	if (ImGui::BeginCombo("##ShowFlags", "Show Flags"))
	{
		// 표시 옵션은 표를 그대로 훑어 체크박스를 만든다.
		// 옵션을 추가할 때 ShowFlags.h의 GShowFlagInfos에만 한 줄 적으면 여기 바로 나온다.
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

	ImGui::Text("FOV     ");
	ImGui::SameLine();
	ImGui::SliderFloat("##FOV", &camera.mFovDegree, 0.0f, 180.0f);

	ImGui::Text("Sensitivity");
	ImGui::SameLine();
	ImGui::SliderFloat("##CameraSensitivity", &camera.Sensitivity, 0.01f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
	


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

	/* Memory Info */
	ImGui::SeparatorText("Memory Info");

	ImGui::Text("Total allocated memory count: %d", UEngineStatics::sTotalAllocationCount);
	ImGui::Text("Total allocated memory size: %d bytes", UEngineStatics::sTotalAllocationBytes);

	/* Gizmo Control */
	ImGui::SeparatorText("Gizmo Control");

	// Display the current gizmo mode dropdown
	const char* gizmoModeNames[] = { "Translate", "Rotate", "Scale" };

	EGIZMO_TYPE currentGizmoType = guiReference.ViewportClient->mGizmo.GetOperation();
	int32 currentGizmoIndex = static_cast<int32>(currentGizmoType);
	if (ImGui::Combo("Gizmo Mode", &currentGizmoIndex, gizmoModeNames, IM_ARRAYSIZE(gizmoModeNames)))
	{
		if (currentGizmoIndex == 0)
		{
			guiReference.ViewportClient->mGizmo.SetOperation(EGIZMO_TYPE::TRANSLATE);
		}
		else if (currentGizmoIndex == 1)
		{
			guiReference.ViewportClient->mGizmo.SetOperation(EGIZMO_TYPE::ROTATE);
		}
		else if (currentGizmoIndex == 2)
		{
			guiReference.ViewportClient->mGizmo.SetOperation(EGIZMO_TYPE::SCALE);
		}
	}
	if (ImGui::Button("Next Gizmo Mode"))
	{
		guiReference.ViewportClient->mGizmo.SetOperation(static_cast<EGIZMO_TYPE>((currentGizmoIndex + 1) % 3));
	}


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
void FSceneManager::drawAssetGrid()
{
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

		// 검색 필터 적용
		if (strlen(mSearchBuffer) > 0 && filename.find(mSearchBuffer) == std::string::npos)
		{
			continue;
		}

		ImGui::PushID(filename.c_str());

		bool isDirectory = entry.is_directory();

		// 아이콘 레이블 (실제 엔진에서는 ImTextureID를 받아 ImGui::ImageButton을 사용)
		const char* iconText = isDirectory ? "[FOLDER]" : "[FILE]";

		// 선택 여부 하이라이트 표시
		if (mSelectedAssetPath == path)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 0.6f));
		}

		ImGui::Button(iconText, ImVec2(mThumbnailSize, mThumbnailSize));

		if (mSelectedAssetPath == path)
		{
			ImGui::PopStyleColor();
		}

		// 클릭 선택
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			mSelectedAssetPath = path;
		}

		// 폴더 더블 클릭 시 이동
		if (isDirectory && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			mCurrentDirectory /= path.filename();
		}

		// ==========================================
		// 에셋 드래그 앤 드롭 (Drag Source)
		// 뷰포트나 인스펙터로 에셋 전송
		// ==========================================
		if (!isDirectory && ImGui::BeginDragDropSource())
		{
			std::string pathString = path.string();

			// "CONTENT_BROWSER_ITEM" 이라는 페이로드 키로 파일 경로 전달
			ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", pathString.c_str(), pathString.size() + 1);

			ImGui::Text("Dragging: %s", filename.c_str());
			ImGui::EndDragDropSource();
		}

		// 파일명 텍스트 표시
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
