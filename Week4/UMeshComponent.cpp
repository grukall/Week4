#include "UMeshComponent.h"
#include "Material.h"

UMeshComponent::UMeshComponent()
{
}

UMeshComponent::~UMeshComponent()
{
	releaseAllInstancedMaterials();
}

void UMeshComponent::SetMaterial(uint32 MaterialSlotIndex, UMaterial* InMaterial)
{
	if(MaterialSlotIndex >= OverrideMaterials.Num()) {
		OverrideMaterials.SetNum(MaterialSlotIndex + 1);
	}
	OverrideMaterials[MaterialSlotIndex] = InMaterial;

	// 슬롯에 다른 머티리얼을 고르면 이전 사본은 의미가 없다. 남겨두면 고른 것과 화면에 보이는 것이
	// 달라진다(사본이 계속 이기기 때문에).
	releaseInstancedMaterial(MaterialSlotIndex);
}

UMaterial* UMeshComponent::GetMaterial(uint32 MaterialSlotIndex) const
{
	if (UMaterial* Instanced = GetInstancedMaterial(MaterialSlotIndex))
	{
		return Instanced;
	}

	if (MaterialSlotIndex >= OverrideMaterials.Num())	return nullptr;
	return OverrideMaterials[MaterialSlotIndex];
}

UMaterial* UMeshComponent::GetInstancedMaterial(uint32 MaterialSlotIndex) const
{
	if (MaterialSlotIndex >= InstancedMaterials.Num())
	{
		return nullptr;
	}

	return InstancedMaterials[MaterialSlotIndex];
}

UMaterial* UMeshComponent::GetMaterialForEdit(uint32 MaterialSlotIndex)
{
	if (UMaterial* Instanced = GetInstancedMaterial(MaterialSlotIndex))
	{
		return Instanced;
	}

	UMaterial* Instance = FObjectFactory::ConstructObject<UMaterial>();
	if (Instance == nullptr)
	{
		return nullptr;
	}

	// 색이나 광택 같은 나머지 값은 원래 머티리얼의 것을 그대로 물려받는다.
	// 바꾸려는 값 하나 때문에 외형 전체가 기본값으로 돌아가면 안 된다.
	if (const UMaterial* SourceMaterial = GetMaterial(MaterialSlotIndex))
	{
		Instance->CopyParametersFrom(*SourceMaterial);
	}

	if (MaterialSlotIndex >= InstancedMaterials.Num())
	{
		InstancedMaterials.SetNum(MaterialSlotIndex + 1);
	}
	InstancedMaterials[MaterialSlotIndex] = Instance;

	return Instance;
}

void UMeshComponent::releaseInstancedMaterial(uint32 MaterialSlotIndex)
{
	if (MaterialSlotIndex >= InstancedMaterials.Num())
	{
		return;
	}

	if (UMaterial* Instance = InstancedMaterials[MaterialSlotIndex])
	{
		Instance->Destroy();
		InstancedMaterials[MaterialSlotIndex] = nullptr;
	}
}

void UMeshComponent::releaseAllInstancedMaterials()
{
	for (uint32 SlotIndex = 0; SlotIndex < InstancedMaterials.Num(); ++SlotIndex)
	{
		if (UMaterial* Instance = InstancedMaterials[SlotIndex])
		{
			Instance->Destroy();
		}
	}

	InstancedMaterials.Empty();
}

void UMeshComponent::ClearMaterialOverride(uint32 MaterialSlotIndex)
{
	releaseInstancedMaterial(MaterialSlotIndex);

	if (MaterialSlotIndex >= OverrideMaterials.Num()) return;

	OverrideMaterials[MaterialSlotIndex] = nullptr;
}

void UMeshComponent::SetTexture(UTexture2D* InTexture)
{
	// 슬롯 0에 들어 있는 머티리얼은 보통 메시 에셋이 준 것이거나 UMaterial::DefaultMaterial이다.
	// 둘 다 씬 전체가 포인터로 공유하는 인스턴스라, 거기에 텍스처를 꽂으면 그 머티리얼을 쓰는 모든
	// 물체의 텍스처가 같이 바뀐다. 전용 사본을 거쳐야 하는 이유다.
	if (UMaterial* Instance = GetMaterialForEdit(0))
	{
		Instance->SetDiffuseTexture(InTexture);
	}
}

UTexture2D* UMeshComponent::GetTexture() const
{
	UMaterial* Material = GetMaterial(0);

	if (Material == nullptr)
	{
		return nullptr;
	}
	UTexture2D* Texture = Material->GetDiffuseTexture();
	return Texture;
}

UMaterial* UMeshComponent::GetOverrideMaterial(uint32 MaterialSlotIndex) const
{
	if (MaterialSlotIndex >= OverrideMaterials.Num())
	{
		return nullptr;
	}

	return OverrideMaterials[MaterialSlotIndex];
}
