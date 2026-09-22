#include "UAtlasAnimationComponent.h"
#include <cmath>

#include "Json/json.hpp"
#include "JsonAssetReference.h"
#include "JsonUtil.h"

void UAtlasAnimationComponent::SerializeClass(json::JSON& outJson) const
{
	Super::SerializeClass(outJson);

	json::JSON& propertiesJson = outJson["Properties"];

	// 아틀라스는 머티리얼 텍스처로도 복원되지만, 그 경로는 머티리얼이 성하다는 전제가 붙는다.
	// 애니메이션이 쓰는 에셋은 여기에 직접 적어둔다.
	propertiesJson["Asset"] = AssetReferenceToJson(Asset);
	propertiesJson["bPlaying"] = bPlaying;
	propertiesJson["bLooping"] = bLooping;
	propertiesJson["bBackward"] = bBackward;
	propertiesJson["Frame"] = Frame;
	propertiesJson["FrameRate"] = FrameRate;
}

void UAtlasAnimationComponent::DeserializeClass(const json::JSON& inJson)
{
	Super::DeserializeClass(inJson);

	const json::JSON& propertiesJson = inJson.at("Properties");

	USpriteAtlas* LoadedAtlas = nullptr;
	if (ReadJsonAssetReference(propertiesJson, "Asset", LoadedAtlas) && LoadedAtlas)
	{
		SetAtlas(LoadedAtlas);
	}
	else
	{
		// 아틀라스 참조가 없는 옛 씬 파일은 머티리얼에 물린 텍스처에서 되짚는다.
		RestoreAtlasState();
	}

	// 플레인 상태를 저장하지 않던 옛 씬 파일에는 이 값들이 없다. 그때 쓰던 고정값으로 채운다.
	if (!propertiesJson.hasKey("mbBillboard"))
	{
		SetBillboard(true);
		SetDepthState(true, false);
	}

	bool bShouldPlay = true;
	ReadJsonBool(propertiesJson, "bPlaying", bShouldPlay);
	ReadJsonBool(propertiesJson, "bLooping", bLooping);
	ReadJsonBool(propertiesJson, "bBackward", bBackward);
	ReadJsonInt(propertiesJson, "FrameRate", FrameRate);
	SetFrameRate(FrameRate);

	int32 StartFrame = 0;
	ReadJsonInt(propertiesJson, "Frame", StartFrame);

	// 프레임 수는 아틀라스가 정한다. 에셋이 바뀌어 프레임이 줄었으면 GetFrameSubUV가 범위를 벗어난다.
	const int32 FrameCount = Asset ? Asset->GetFrameCount() : 0;
	if (StartFrame < 0 || StartFrame >= FrameCount)
	{
		StartFrame = 0;
	}

	Play(StartFrame, bLooping, bBackward);

	if (!bShouldPlay)
	{
		Pause();
	}

	if (Asset && FrameCount > 0)
	{
		mSubUV = Asset->GetFrameSubUV(Frame);
	}
}

UAtlasAnimationComponent::UAtlasAnimationComponent()
{
}

void UAtlasAnimationComponent::Initialize(USpriteAtlas* textureAsset)
{
	UPlaneComponent::Initialize();

	SetAtlas(textureAsset);

	//기본 블랜드 모드는 Additive
	mBlendMode = ERenderBlendMode::Additive;
}

void UAtlasAnimationComponent::RestoreAtlasState()
{
	Asset = nullptr;

	UTexture2D* Texture = GetTexture();
	if (!Texture)
	{
		return;
	}

	// 스프라이트 아틀라스가 아니면 애니메이션 상태를 복원할 게 없다.
	Asset = Texture->Cast<USpriteAtlas>();
	if (!Asset)
	{
		return;
	}

	mBlendMode = ERenderBlendMode::Additive;

	Frame = 0;
	FrameAccumulator = 0.f;

	if (Asset->GetFrameCount() > 0)
	{
		mSubUV = Asset->GetFrameSubUV(Frame);
	}
}


void UAtlasAnimationComponent::SetAtlas(USpriteAtlas* InAtlas)
{
	Asset = InAtlas;
	SetTexture(InAtlas);

	Frame = 0;
	FrameAccumulator = 0.f;
	mSubUV = InAtlas ? InAtlas->GetFrameSubUV(0) : FVector4(0.f, 0.f, 1.f, 1.f);
}

void UAtlasAnimationComponent::Play(int32 StartFrame, bool bIsLooping, bool bBackwardAnimate)
{
	bPlaying = true;
	bLooping = bIsLooping;
	Frame = StartFrame;
	bBackward = bBackwardAnimate;
}

void UAtlasAnimationComponent::Pause()
{
	bPlaying = false;
}

void UAtlasAnimationComponent::Resume()
{
	bPlaying = true;
}

void UAtlasAnimationComponent::Reset()
{
	Frame = 0;
	mSubUV = Asset->GetFrameSubUV(0);
	FrameAccumulator = 0.f;
	Pause();
}

void UAtlasAnimationComponent::Tick(float deltaTime)
{
	Super::Tick(deltaTime);

	if (!bPlaying || !Asset)
	{
		return;
	}

	const int32 FrameCount = Asset->GetFrameCount();
	if (FrameCount <= 0)
	{
		return;
	}

	FrameAccumulator += deltaTime * FrameRate;

	//첫 줄: int32로 캐스팅하면 소수점이 잘립니다. 1.2 → 1. 지금 넘길 수 있는 온전한 프레임 수입니다.
	//둘째 줄 : 방금 쓴 만큼을 빼서 소수부만 남깁니다. 1.2 - 1 = 0.2.이 0.2가 다음 틱으로 이월됩니다.
	const int32 Advance = static_cast<int32>(FrameAccumulator);
	FrameAccumulator -= static_cast<float>(Advance);

	if (Advance > 0)
	{
		const int32 NextFrame = Frame + (bBackward ? -Advance : Advance);

		if (!bLooping && (NextFrame >= FrameCount || NextFrame < 0))
		{
			Frame = bBackward ? 0 : FrameCount - 1;
			mSubUV = Asset->GetFrameSubUV(Frame);
			bPlaying = false;
			return;
		}

		Frame = ((NextFrame % FrameCount) + FrameCount) % FrameCount;
	}

	mSubUV = Asset->GetFrameSubUV(Frame);
}
