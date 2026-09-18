#pragma once

#include "PrimitiveComponent.h"

class UTexture2D;

// 메시를 사용하는 컴포넌트의 공통 Material 단계.
// UStaticMesh가 가진 기본 Material을 사용할 수도 있고,
// 이 컴포넌트에서만 Override Material을 지정할 수도 있다.
class UMeshComponent : public UPrimitiveComponent
{
	REFLECT_CLASS(UMeshComponent, UPrimitiveComponent);

public:
	UMeshComponent();

	using UPrimitiveComponent::Initialize;

	void SetMaterial(uint32 MaterialSlotIndex, UMaterial* InMaterial);
	virtual UMaterial* GetMaterial(uint32 MaterialSlotIndex) const;

	void ClearMaterialOverride(uint32 MaterialSlotIndex);

	void SetTexture(UTexture2D* InTexture);
	UTexture2D* GetTexture() const;
protected:
	TArray<UMaterial*> OverrideMaterials;
};
