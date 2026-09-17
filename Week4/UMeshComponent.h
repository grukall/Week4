#pragma once

#include "PrimitiveComponent.h"

class UTexture2D;

// 재질(지금은 텍스처 한 장)을 가진 프리미티브의 공통 단계.
// 어떤 메시를 그리는지는 이 단계가 알지 않는다. 파생 클래스가 자기 메시로 그린다.
class UMeshComponent : public UPrimitiveComponent
{
	REFLECT_CLASS(UMeshComponent, UPrimitiveComponent);

public:
	UMeshComponent();

	using UPrimitiveComponent::Initialize;

	//TODO : UMaterial이 들어오면 OverrideMaterials 배열로 바뀐다.
	// 메시는 여러 컴포넌트가 공유하므로, 이 컴포넌트만 다른 텍스처를 쓰려면 여기에 건다.
	inline void SetTexture(UTexture2D* InTexture) { TextureOverride = InTexture; }
	inline UTexture2D* GetTexture() const { return TextureOverride; }

protected:
	UTexture2D* TextureOverride = nullptr;
};
