#pragma once
#include "EditorPanel.h"
#include "Actor.h"

class USceneComponent;
class UWorld;

class FPropertyPanel : public IEditorPanel
{
public:
	FPropertyPanel() = default;
	~FPropertyPanel() = default;

	bool Init() override;
	void Tick(float DeltaTime)override;
	void OnRender() override;

	void SetTarget(AActor* InTarget) { Target = InTarget; }
	void SetWorld(UWorld* InWorld) { World = InWorld; }

private:
	UWorld* World;
	AActor* Target;
	ImFont* CustomFont;
};

