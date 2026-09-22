#pragma once

#include "PrimitiveComponent.h"

class UTexture2D;

// 메시를 사용하는 컴포넌트의 공통 Material 단계.
// UStaticMesh가 가진 기본 Material을 사용할 수도 있고,
// 이 컴포넌트에서만 Override Material을 지정할 수도 있다.
//
// 슬롯 하나에 두 가지가 들어갈 수 있다.
//  - OverrideMaterials[slot] : 이 슬롯에 "고른" 머티리얼. 보통 여러 컴포넌트가 공유하는 에셋이다.
//  - InstancedMaterials[slot] : 이 컴포넌트에서만 값을 바꾸려고 만든 전용 사본.
// 공유 에셋을 직접 고치면 그 에셋을 쓰는 씬의 모든 물체가 같이 바뀌므로, 컴포넌트 단위로 달라져야
// 하는 값(텍스처, UV 속도 등)은 반드시 GetMaterialForEdit을 거쳐 사본에 쓴다.
class UMeshComponent : public UPrimitiveComponent
{
	REFLECT_CLASS(UMeshComponent, UPrimitiveComponent);

public:
	UMeshComponent();
	virtual ~UMeshComponent();

	using UPrimitiveComponent::Initialize;

	// 슬롯에 머티리얼을 고른다. 그 슬롯의 전용 사본은 버려진다 — 새로 고른 쪽이 우선이다.
	void SetMaterial(uint32 MaterialSlotIndex, UMaterial* InMaterial);

	// 실제로 그릴 때 쓰는 머티리얼. 전용 사본이 있으면 그것, 없으면 고른 머티리얼.
	virtual UMaterial* GetMaterial(uint32 MaterialSlotIndex) const;

	// 이 컴포넌트에서만 바뀌어야 하는 값을 고칠 때 쓴다. 처음 부를 때 현재 머티리얼의 사본을
	// 만들고, 그 뒤로는 같은 사본을 돌려준다. 공유 에셋은 절대 수정되지 않는다.
	UMaterial* GetMaterialForEdit(uint32 MaterialSlotIndex);

	// 이 슬롯이 전용 사본을 갖고 있으면 그것, 아니면 nullptr.
	// 사본은 디스크의 에셋이 아니므로 저장할 때 참조가 아니라 값으로 적어야 한다.
	UMaterial* GetInstancedMaterial(uint32 MaterialSlotIndex) const;

	void ClearMaterialOverride(uint32 MaterialSlotIndex);

	void SetTexture(UTexture2D* InTexture);
	UTexture2D* GetTexture() const;

	UMaterial* GetOverrideMaterial(uint32 MaterialSlotIndex) const;

protected:
	// 전용 사본은 이 컴포넌트가 만들었으므로 수명도 여기서 책임진다.
	void releaseInstancedMaterial(uint32 MaterialSlotIndex);
	void releaseAllInstancedMaterials();

	TArray<UMaterial*> OverrideMaterials;
	TArray<UMaterial*> InstancedMaterials;
};
