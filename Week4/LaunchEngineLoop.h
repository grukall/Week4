#pragma once

#include <Windows.h>
#include "FrameTimer.h"
#include "FEditorViewportClient.h"
#include "Camera.h"
#include "SceneManager.h"
#include "FileManager.h"
#include "Renderer.h"
#include "World.h"
#include "FAssetManager.h"
#include "FFontManager.h"
#include "FComponentVisualizer.h"

#include <d3d11.h>

class Sphere;
class FGraphicsManager;
class FStatManager;
class FEngineLoop
{
public:
	FEngineLoop()
	{
	}
	~FEngineLoop() {};

	void Init(HINSTANCE hInstance, WNDPROC WndProc, const char* CmdLine);
	void Tick(bool bPumpMessages);
	void End();

	FAssetManager* GetAssetManager() { return mAssetManager; }
	FStatManager* GetStatManager() { return mStatManager; }
	TArray<FEditorViewportClient*>& GetViewportClients() { return ViewportClients; }
	FEditorViewportClient* GetActiveViewportClient() { return ActiveViewportClient; }

private:
	void InitAssetManager();
	void InitStatManager();

private:
	// Todo: Make as pointer
	FFrameTimer* FrameTimer;
	bool GInTick = false;

	TArray<FEditorViewportClient*> ViewportClients;
	FEditorViewportClient* ActiveViewportClient = nullptr;

	FGraphicsManager* mGraphicsManager;
	FSceneManager* mSceneManager;
	FFileManager* mFileManager;
	FAssetManager* mAssetManager;
	FFontManager* mFontManager;

	FComponentVisualizerManager* mComponentVisualizerManager;
	FStatManager* mStatManager;
};

inline FEngineLoop GEngineLoop;
