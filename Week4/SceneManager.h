#pragma once

#include <string_view>
#include <filesystem>
#include <string>
#include "TArray.h"
#include "RenderInfo.h"
#include "enum.h"
#include "FAssetManager.h"
#include "PropertyPanel.h"
#include "SWindow.h"

inline constexpr std::string_view kSceneDataDir = "SceneData\\";
inline constexpr std::string_view kSceneDataSuffix = ".Scene";

// 씬 파일 포맷 버전. 저장하는 쪽이 항상 이 값을 찍고, 읽는 쪽은 이보다 높은 파일을 거부한다.
// 필드를 지우거나 의미를 바꾸는 변경을 하면 올리고, 로더에 그 버전용 처리를 추가한다.
// (0은 버전을 찍지 않던 시절의 파일이다. 키가 늘기만 했으므로 그대로 읽힌다.)
inline constexpr uint32 kSceneFormatVersion = 1;

class FFileManager;
class FFrameTimer;
class FViewport;
class FCamera;
struct FEditorViewportClient; // 실제 정의가 struct – class로 선언하면 MSVC 맹글링이 달라져 링크 실패
class FGraphicsManager;
class UWorld;
class FThumbnailManager;

struct FGuiReference
{
	const FFrameTimer& FrameTimer;
	FGraphicsManager* GraphicsManager;
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
	explicit FSceneManager(const TArray<FViewport*>& inViewports);
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

	// runtimeCamera는 씬 파일에 담을 수 없는 것(빌보드 카메라 등)을 다시 묶는 데 쓰인다.
	// 넘기지 않아도 로드 자체는 되지만, 빌보드는 카메라를 받을 때까지 정지 상태가 된다.
	void LoadScene(const std::filesystem::path& scenePath, const FFileManager& fileManager, FCamera* runtimeCamera = nullptr);

	void SaveConfig(const char* IniPath = ".\\editor.ini");
	void LoadConfig(const char* IniPath = ".\\editor.ini");

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
	int mMaximizedViewportIndex = -1;

	void updateControlPanelGUI(const FGuiReference& guiReference);

	//TODO: PropertyWindow에 표시하는 정보를 다루는 구조체 및 시스템이 후에 필요하다.
	//지금은 하드코딩
	void updatePropertyWindowGUI(const FGuiReference& guiReference);
	void updateOutlinerGUI(const FGuiReference& guiReference);
};
