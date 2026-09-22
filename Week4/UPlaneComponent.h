#pragma once

#include "UStaticMeshComponent.h"
#include "Assets.h"
#include "Camera.h"
#include "Actor.h"
#include "FAssetManager.h"
#include "ShowFlags.h"
#include "RenderInfo.h"
#include "Json/json.hpp"
#include "JsonUtil.h"

class UPlaneComponent : public UStaticMeshComponent
{
	REFLECT_CLASS(UPlaneComponent, UStaticMeshComponent)

public:
	UPlaneComponent()
	{
		SetStaticMesh(FAssetManager::Get().GetAssetAs<UStaticMesh>(FName("PlaneMesh"), true));
	}

	void SerializeClass(json::JSON& outJson) const override
	{
		Super::SerializeClass(outJson);

		json::JSON& propertiesJson = outJson["Properties"];
		propertiesJson["mSubUV"] = FVector4ToJson(mSubUV);
		propertiesJson["mBlendMode"] = static_cast<int32>(mBlendMode);
		propertiesJson["mbBillboard"] = mbBillboard;
		propertiesJson["mEnableDepthTest"] = mEnableDepthTest;
		propertiesJson["mEnableDepthWrite"] = mEnableDepthWrite;
	}

	void DeserializeClass(const json::JSON& inJson) override
	{
		Super::DeserializeClass(inJson);

		const json::JSON& propertiesJson = inJson.at("Properties");

		ReadJsonVector4(propertiesJson, "mSubUV", mSubUV);
		ReadJsonBool(propertiesJson, "mbBillboard", mbBillboard);
		ReadJsonBool(propertiesJson, "mEnableDepthTest", mEnableDepthTest);
		ReadJsonBool(propertiesJson, "mEnableDepthWrite", mEnableDepthWrite);

		// 열거형은 파일에 정수로 들어간다. 범위를 벗어난 값이 렌더러의 상태 배열을 넘어가지 않게 막는다.
		int32 BlendMode = static_cast<int32>(mBlendMode);
		ReadJsonInt(propertiesJson, "mBlendMode", BlendMode);
		if (BlendMode >= 0 && BlendMode < static_cast<int32>(ERenderBlendMode::Count))
		{
			mBlendMode = static_cast<ERenderBlendMode>(BlendMode);
		}
	}

	void PostSceneLoad(const FSceneLoadContext& context) override
	{
		Super::PostSceneLoad(context);

		// 카메라는 씬 파일에 담기지 않는다. 빌보드를 쓰는 모든 플레인이 여기서 다시 묶인다.
		if (context.Camera)
		{
			SetBillboardCamera(*context.Camera);
		}
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
