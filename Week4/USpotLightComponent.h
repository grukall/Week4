#pragma once

#include "SceneComponent.h"
#include "Actor.h"
#include "MathUtility.h"

class USpotLightComponent : public USceneComponent
{
	REFLECT_CLASS(USpotLightComponent, USceneComponent)

public:
	void Tick(float DeltaTime) override
	{
		// NOTE: SpotLightComponent의 위치와 회전을 부모 액터에 맞춘다. 현재 Hierarchy가 없으므로 부모 액터의 위치와 회전만 가져와서 적용한다.
		FTransform ParentTransform = mOwner->GetTransform();
		SetRelativeLocation(ParentTransform.Location);
		SetRelativeRotation(ParentTransform.Rotation);
		SetRelativeScale3D(ParentTransform.Scale);
	}

	inline float GetRange() const { return Range; }
	inline float GetInnerConeAngle() const { return mInnerConeAngle; }
	inline float GetOuterConeAngle() const { return mOuterConeAngle; }
	inline const FVector4& GetColor() const { return mColor; }

	inline void SetColor(const FVector4& InColor) { mColor = InColor; }

	inline void SetOuterConeAngle(float InAngle)
	{
		mOuterConeAngle = FMath::Clamp(InAngle, 0.f, MAX_CONE_ANGLE);
		mInnerConeAngle = FMath::Min(mInnerConeAngle, mOuterConeAngle);
	}

	inline void SetInnerConeAngle(float InAngle)
	{
		mInnerConeAngle = FMath::Clamp(InAngle, 0.f, mOuterConeAngle);
	}

private:
	static constexpr float MAX_CONE_ANGLE = 89.f;

	float Range = 5.0f;
	FVector4 mColor = { 1.f, 1.f, 1.f, 1.f };
	float mInnerConeAngle = 30.0f;
	float mOuterConeAngle = 45.0f;
};
