#pragma once

#include <string_view>
#include <filesystem>
#include <string>
#include "SceneData.h"
#include "TArray.h"
#include "RenderInfo.h"
#include "enum.h"
#include "FAssetManager.h"
#include "PropertyPanel.h"
#include "SWindow.h"

inline constexpr std::string_view kSceneDataDir = "SceneData\\";
inline constexpr std::string_view kSceneDataSuffix = ".Scene";

class FFileManager;
class FFrameTimer;
class FEditorViewportClient;
class FGraphicsManager;
class UWorld;

struct FGuiReference
{
	const FFrameTimer& FrameTimer;
	FGraphicsManager* GraphicsManager;
	const TArray<FEditorViewportClient*>* ViewportClients;
	FEditorViewportClient*& ActiveViewport;
	const FFileManager* FileManager;
	FAssetManager* AssetManager;
};

struct FGuiInputField
{
	/* Spawn Actor */
	int32 ActorTypeIndex = 0;
	int32 SpawnCount = 1;

	/* Scene Control */
	char SceneName[512] = "Default";

	/* Object Lists */
	TArray<AActor*> SortedActorLists;
	uint64 LastGUObjectRevision = -1;
};

class FSceneManager
{
public:
	FSceneManager();
	~FSceneManager();

	void Tick(float deltaTime);
	void Update(float deltaTime, FRenderCollector& outCollector);
	void UpdateGUI(const FGuiReference& guiReference);


	const TArray<FRenderInfo> GetAxisRenderInfos();

	// Clear world
	void NewScene();
	void DeleteScene();

	// 파일 탐색기용 오버로드
	void SaveScene(const std::filesystem::path& scenePath, const FFileManager& fileManager);
	void LoadScene(const std::filesystem::path& scenePath, const FFileManager& fileManager);

	UWorld* GetCurrentWorld() const { return mCurrentWorld; }

	AActor* GetSelectedActor() const { return mSelectedActor; }
	bool IsActorSelected() const { return mSelectedActor != nullptr; }
	void SetSelectedActor(AActor* actor);
	void ResetSelectedActor() { mSelectedActor = nullptr; }

	float GetPanelWidth() const;
	float GetViewportX() const { return mViewportX; }
	float GetViewportY() const { return mViewportY; }
	float GetViewportWidth() const { return mViewportWidth; }
	float GetViewportHeight() const { return mViewportHeight; }
	bool IsViewportHovered() const { return mbViewportHovered; }
	SWindow* GetRootWindow() const { return mRootWindow; }

	void InitObjViewer(const char* CmdLine);
	void UpdateObjViewerGUI(const FGuiReference& guiReference);
private:
	static constexpr float MIN_WIDTH_RATIO = 0.2f;
	static constexpr float MAX_WIDTH_RATIO = 0.6f;

	static constexpr float CONTROL_PANEL_HEIGHT_RATIO = 0.4f;
	static constexpr float WINDOW_PROPERTY_HEIGHT_RATIO = 0.3f;

	float mPanelWidth;
	float mViewportX;
	float mViewportY;
	float mViewportWidth;
	float mViewportHeight;
	bool mbViewportHovered = false;

	UWorld* mCurrentWorld = nullptr;
	AActor* mSelectedActor = nullptr;
	FGuiInputField mGuiInputField;

	FPropertyPanel* mPropertyPanel = nullptr;

	//Content Browser
	std::filesystem::path mRootPath = "Assets";
	std::filesystem::path mCurrentDirectory = "Assets";
	std::filesystem::path mSelectedAssetPath = "";
	char mSearchBuffer[256] = "";
	float mThumbnailSize = 64.0f;
	bool mShowContentBrowser = false;
	void updateContentBrowserGUI(const FGuiReference& guiReference);
	void drawFolderTree(const std::filesystem::path& currentPath);
	void drawAssetGrid();

	SWindow* mRootWindow;

	void updateControlPanelGUI(const FGuiReference& guiReference);

	//TODO: PropertyWindow에 표시하는 정보를 다루는 구조체 및 시스템이 후에 필요하다.
	//지금은 하드코딩
	void updatePropertyWindowGUI(const FGuiReference& guiReference);
	void updateOutlinerGUI(const FGuiReference& guiReference);
};
