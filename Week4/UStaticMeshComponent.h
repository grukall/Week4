#pragma once

#include "UMeshComponent.h"
#include "Material.h"

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

	void SetStaticMesh(UStaticMesh* _InStaticMesh);
	UStaticMesh* GetStaticMesh() const { return StaticMesh; }

	void SetMaterial(uint32 MaterialSlotIndex, UMaterial* InMaterial);

	UMaterial* GetMaterial(uint32 MaterialSlotIndex) const override;

	void ClearMaterials();

	// 메시와 오버라이드 머티리얼을 한 곳에서 처리한다. SetStaticMesh가 OverrideMaterials를
	// 메시 기본값으로 덮어쓰므로, 둘의 적용 순서를 쥐고 있어야 오버라이드가 날아가지 않는다.
	virtual void SerializeClass(json::JSON& outJson) const override;
	virtual void DeserializeClass(const json::JSON& inJson) override;


	// 섹션(머티리얼 구간) 하나당 RenderInfo 하나를 만든다.
	virtual void GetRenderInfos(TArray<FRenderInfo>* outRenderInfos) const override;

	// AABB로 먼저 거르고 메시의 삼각형과 판정한다.
	virtual bool RayCastComponent(const FPickingRay& PickingRay, float& OutHitT) const override;
	
	inline const TArray<UMaterial*>& GetMaterials() const {
		return OverrideMaterials;
	}
protected:
	UStaticMesh* StaticMesh = nullptr;
};
