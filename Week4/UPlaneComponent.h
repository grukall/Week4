#pragma once

#include "UStaticMeshComponent.h"
#include "Assets.h"
#include "Camera.h"
#include "Actor.h"
#include "FAssetManager.h"
#include "ShowFlags.h"
#include "RenderInfo.h"

class UPlaneComponent : public UStaticMeshComponent
{
	REFLECT_CLASS(UPlaneComponent, UStaticMeshComponent)

public:
	UPlaneComponent()
	{
		SetStaticMesh(FAssetManager::Get().GetAssetAs<UStaticMesh>(FName("PlaneMesh"), true));
	}

	void Tick(float DeltaTime) override
	{
		// NOTE: SpotLightComponent의 위치와 회전을 부모 액터에 맞춘다. 현재 Hierarchy가 없으므로 부모 액터의 위치와 회전만 가져와서 적용한다.
		FTransform ParentTransform = mOwner->GetTransform();
		SetRelativeLocation(ParentTransform.Location);
		SetRelativeRotation(ParentTransform.Rotation);

		if (mBillboardCamera && mbBillboard)
		{
			FTransform PivotTransform = GetTransformMatrix();
			FRotator Rotation = FRotator::LookAt(PivotTransform.Location, PivotTransform.Location + mBillboardCamera->GetForwardVector());
			SetRelativeRotation(Rotation);
		}
	}

	void Render(FRenderCollector& RenderCollector) override
	{
		if (!FShowFlags::Get().IsEnabled(EShowFlag::Primitive))
		{
			return;
		}

		FTransform PivotTransform = GetTransformMatrix();

		FRenderQuadInfo QuadInfo;
		QuadInfo.Model = PivotTransform.MakeMatrix();
		QuadInfo.Color = FVector4(1.f, 1.f, 1.f, 1.f);
		UTexture2D* QuadTexture = GetTexture();
		QuadInfo.TextureSRV = QuadTexture ? QuadTexture->GetSRV() : nullptr;
		QuadInfo.SubUV = mSubUV;
		QuadInfo.BlendMode = mBlendMode;
		QuadInfo.EnableDepthTest = mEnableDepthTest;
		QuadInfo.EnableDepthWrite = mEnableDepthWrite;
		QuadInfo.bIsBillboard = mbBillboard;

		RenderCollector.AddQuadInfo(QuadInfo);
	}

	inline void SetBillboardCamera(FCamera& camera) { mBillboardCamera = &camera; }
	inline void SetBillboard(bool billboard) { mbBillboard = billboard; }
	inline void SetDepthState(bool enableDepthTest, bool enableDepthWrite) { mEnableDepthTest = enableDepthTest; mEnableDepthWrite = enableDepthWrite; }
	void SetBlendState(ERenderBlendMode InBlendMode) { mBlendMode = InBlendMode; }

protected:
	FVector4 mSubUV = { 0.f, 0.f, 1.f, 1.f };
	ERenderBlendMode mBlendMode = ERenderBlendMode::Opaque;

private:
	FCamera* mBillboardCamera = nullptr;
	bool mbBillboard = false;
	bool mEnableDepthTest = true;
	bool mEnableDepthWrite = true;
};
