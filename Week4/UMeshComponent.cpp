#include "UMeshComponent.h"
#include "Material.h"
UMeshComponent::UMeshComponent()
{
}

void UMeshComponent::SetMaterial(uint32 MaterialSlotIndex, UMaterial* InMaterial)
{
	if(MaterialSlotIndex >= OverrideMaterials.Num()) {
		OverrideMaterials.SetNum(MaterialSlotIndex + 1);
	}
	OverrideMaterials[MaterialSlotIndex] = InMaterial;
}

UMaterial* UMeshComponent::GetMaterial(uint32 MaterialSlotIndex) const
{
	if (MaterialSlotIndex >= OverrideMaterials.Num())	return nullptr;
	return OverrideMaterials[MaterialSlotIndex];
}

void UMeshComponent::ClearMaterialOverride(uint32 MaterialSlotIndex)
{
	if (MaterialSlotIndex >= OverrideMaterials.Num()) return;

	OverrideMaterials[MaterialSlotIndex] = nullptr;
}

void UMeshComponent::SetTexture(UTexture2D* InTexture)
{
	if (OverrideMaterials.Num() == 0)
	{
		OverrideMaterials.SetNum(1);
	}

	if (OverrideMaterials[0] == nullptr)
	{
		OverrideMaterials[0] = FObjectFactory::ConstructObject<UMaterial>();

		if (OverrideMaterials[0] == nullptr)
		{
			return;
		}
	}

	OverrideMaterials[0]->SetDiffuseTexture(InTexture);
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
