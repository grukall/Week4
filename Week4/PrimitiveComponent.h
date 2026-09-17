#pragma once

#include "SceneComponent.h"
#include "Assets.h"
#include "RayCast.h"

class UPrimitiveComponent : public USceneComponent
{
	REFLECT_CLASS(UPrimitiveComponent, USceneComponent)

public:
	UPrimitiveComponent();

	using USceneComponent::Initialize;
	void Initialize();
	void Initialize(FVector location, FRotator rotation, FVector scale3D);

	virtual ~UPrimitiveComponent();

	virtual void SerializeClass(json::JSON& outJson) const override;
	virtual void DeserializeClass(const json::JSON& inJson) override;

	virtual void Render(FRenderCollector& RenderCollector) override;
	virtual void GetRenderInfos(TArray<FRenderInfo>* outRenderInfos) const override;
	virtual void RegisterPickTarget(FRenderCollector& RenderCollector) override;

	virtual bool RayCastComponent(const FPickingRay& PickingRay, float& OutHitT) const;

protected:
	void RestoreMeshAsset();

};



