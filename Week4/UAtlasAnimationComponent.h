#pragma once

#include "UPlaneComponent.h"
#include <wrl/client.h>
#include "Assets.h"

class UAtlasAnimationComponent : public UPlaneComponent
{
	REFLECT_CLASS(UAtlasAnimationComponent, UPlaneComponent)

public:
	UAtlasAnimationComponent();

	using UPlaneComponent::Initialize;
	void Initialize(USpriteAtlas* textureAsset);

	void DeserializeClass(const json::JSON& inJson) override
	{
		Super::DeserializeClass(inJson);

		RestoreAtlasState();


		SetBillboard(true);
		SetDepthState(true, false);

		Play();
	}
	void RestoreRuntimeCamera(FCamera& Camera)
	{
		SetBillboardCamera(Camera);
	}
	void SetAtlas(USpriteAtlas* InAtlas);
	inline USpriteAtlas* GetAtlas() const { return Asset; }

	void Play(int32 StartFrame = 0, bool bIsLooping = true, bool bBackwardAnimate = false);
	void Pause();
	void Resume();
	void Reset();
	void SetLooping(bool bInLooping) { bLooping = bInLooping; }

	inline bool IsPlaying() const { return bPlaying; }
	inline bool IsLooping() const { return bLooping; }
	inline bool IsBackward() const { return bBackward; }
	inline int32 GetFrameRate() const { return FrameRate; }
	
	//초당 표현되는 프레임 수
	void SetFrameRate(int32 InFrameRate) { FrameRate = FMath::Max(InFrameRate, 1); }

	virtual void Tick(float deltaTime) override;
private:
	void RestoreAtlasState();
	USpriteAtlas* Asset = nullptr;
	bool bPlaying = false;
	bool bLooping = true;
	bool bBackward = false;
	int32 Frame = 0;
	int32 FrameRate = 36;
	float FrameAccumulator = 0.f;
};
