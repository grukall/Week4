#pragma once

#include "UMeshComponent.h"

class UStaticMesh;

class UStaticMeshComponent : public UMeshComponent
{
	REFLECT_CLASS(UStaticMeshComponent, UMeshComponent);

	REFLECT_START(className)
	PROPERTY(StaticMesh)
	REFLECT_END()
public:

	UStaticMeshComponent();

	// 이 선언이 없으면 아래 Initialize가 부모의 Initialize들을 전부 가린다.
	using UMeshComponent::Initialize;
	void Initialize(UStaticMesh* InStaticMesh);

	void SetStaticMesh(UStaticMesh* _InStaticMesh) { StaticMesh = _InStaticMesh; }
	UStaticMesh* GetStaticMesh() const { return StaticMesh; }

	void SetMaterial(uint32 MaterialSlotIndex, UMaterial* InMaterial);

	UMaterial* GetMaterial(uint32 MaterialSlotIndex) const override;




	// 섹션(머티리얼 구간) 하나당 RenderInfo 하나를 만든다.
	virtual void GetRenderInfos(TArray<FRenderInfo>* outRenderInfos) const override;

	// AABB로 먼저 거르고 메시의 삼각형과 판정한다.
	virtual bool RayCastComponent(const FPickingRay& PickingRay, float& OutHitT) const override;

protected:
	UStaticMesh* StaticMesh = nullptr;
	
	TArray
		
		<UMaterial*> OverrideMaterials;
};
