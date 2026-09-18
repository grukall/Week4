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

	// 오버라이드가 없으면 메시가 들고 있는 값으로 떨어진다.
	UTexture2D* GetRenderTexture() const;
	FVector4 GetRenderColor() const;

	// 섹션(머티리얼 구간) 하나당 RenderInfo 하나를 만든다.
	virtual void GetRenderInfos(TArray<FRenderInfo>* outRenderInfos) const override;

	// AABB로 먼저 거르고 메시의 삼각형과 판정한다.
	virtual bool RayCastComponent(const FPickingRay& PickingRay, float& OutHitT) const override;

protected:
	UStaticMesh* StaticMesh = nullptr;
	
	//오버라이드할 머티리얼, UStaticMesh의 멀티 머테리얼 인덱스 순으로 적용
	//TArray<UMaterial*> OverrideMaterials;
};
