#include "UStaticMeshComponent.h"

#include "Actor.h"
#include "Assets.h"
#include "EngineMathLibrary.h"
#include "RenderInfo.h"

UStaticMeshComponent::UStaticMeshComponent()
{
}

void UStaticMeshComponent::Initialize(UStaticMesh* InStaticMesh)
{
	UMeshComponent::Initialize();

	StaticMesh = InStaticMesh;
}

void UStaticMeshComponent::SetStaticMesh(UStaticMesh* _InStaticMesh)
{
	if (GetStaticMesh() == _InStaticMesh) {
		return;
	}

	StaticMesh = _InStaticMesh;
	OverrideMaterials.Empty();

	if (StaticMesh) {
		const TArray<UMaterial*>& MeshMaterials = StaticMesh->GetMaterials();

		for (uint32 i = 0; i < MeshMaterials.Num(); ++i) {
			SetMaterial(i, MeshMaterials[i]);
		}
	}
}

/*UTexture2D* UStaticMeshComponent::GetRenderTexture() const
{
	if (TextureOverride)
	{
		return TextureOverride;
	}

	return StaticMesh ? StaticMesh->GetTexture() : nullptr;
}

FVector4 UStaticMeshComponent::GetRenderColor() const
{
	return StaticMesh ? StaticMesh->GetColor() : FVector4(1.f, 1.f, 1.f, 1.f);
}*/

void UStaticMeshComponent::SetMaterial(uint32 MaterialSlotIndex, UMaterial* InMaterial)
{
	if (MaterialSlotIndex >= OverrideMaterials.Num()) {
		OverrideMaterials.SetNum(MaterialSlotIndex + 1);
	}

	OverrideMaterials[MaterialSlotIndex] = InMaterial;
}

UMaterial* UStaticMeshComponent::GetMaterial(uint32 MaterialSlotIndex) const
{

	if (OverrideMaterials.Num() > MaterialSlotIndex) {
		return OverrideMaterials[MaterialSlotIndex];
	}

	if (!StaticMesh)	return nullptr;

	return StaticMesh->GetMaterial(MaterialSlotIndex);
}

void UStaticMeshComponent::ClearMaterials()
{
	OverrideMaterials.Empty();
}

void UStaticMeshComponent::GetRenderInfos(TArray<FRenderInfo>* outRenderInfos) const
{
	assert(outRenderInfos);

	if (!StaticMesh)
	{
		return;
	}

	const TArray<FStaticMeshSection>& Sections = StaticMesh->GetSections();
	if (Sections.IsEmpty())
	{
		return;
	}

	const FMatrix WorldMatrix = GetTransformMatrix().MakeMatrix();
	const FObjectID ObjectID = mOwner
		? FObjectID{ mOwner->UUID, mOwner->InternalIndex }
		: FObjectID{ 0, 0 };

		for (uint32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex) {
			const FStaticMeshSection& Section = Sections[SectionIndex];

			UMaterial* Material = GetMaterial(Section.MaterialSlotIndex);

			FRenderInfo RenderInfo{};

			RenderInfo.StaticMesh = StaticMesh;
			RenderInfo.WorldTransformMatrix = WorldMatrix;
			RenderInfo.ObejctID = ObjectID;
			RenderInfo.SectionIndex = SectionIndex;

			RenderInfo.Material = Material;
			if (!RenderInfo.Material) {
				RenderInfo.Material = UMaterial::DefaultMaterial;
			}
			outRenderInfos->Add(RenderInfo);
	}
}

bool UStaticMeshComponent::RayCastComponent(const FPickingRay& PickingRay, float& OutHitT) const
{
	if (!StaticMesh)
	{
		return false;
	}

	const FMatrix WorldMatrix = GetTransformMatrix().MakeMatrix();

	// AABB 충돌체를 이용한 광선-메시 충돌 최적화
	const FAABB BoundingBox = StaticMesh->GetLocalBoundingBox().ToWorld(WorldMatrix);
	if (!RayIntersectsAABB(PickingRay.ToRay(), PickingRay.Length, BoundingBox))
	{
		return false;
	}

	// 메시 충돌체를 이용한 광선-삼각형 충돌 판정
	const TArray<FVertexSimple>& Vertices = StaticMesh->GetVertices();
	const TArray<uint32>& Indices = StaticMesh->GetIndices();
	if (Vertices.IsEmpty() || Indices.IsEmpty())
	{
		return false;
	}

	const FMatrix WorldToLocal = WorldMatrix.AffineInverse();
	if (WorldToLocal == FMatrix::Zero)
	{
		// 역행렬이 존재하지 않으면(스케일이 작아 det이 0에 가까운 경우) RayCast 대상에서 제외
		return false;
	}

	const FVector LocalNear = WorldToLocal.TransformPosition(PickingRay.Near);
	const FVector LocalFar = WorldToLocal.TransformPosition(PickingRay.Far);

	bool bHit = false;
	float NearestT = FLT_MAX;

	// 삼각형 리스트라 정점 3개씩 묶인다
	for (uint32 i = 0; i + 2 < Indices.Num(); i += 3)
	{
		const FVector V0 = Vertices[Indices[i]].GetPosition();
		const FVector V1 = Vertices[Indices[i + 1]].GetPosition();
		const FVector V2 = Vertices[Indices[i + 2]].GetPosition();

		float OutT, OutU, OutV;
		if (RayIntersectsTriangle(LocalNear, LocalFar, V0, V1, V2, OutT, OutU, OutV) && OutT < NearestT)
		{
			// 같은 메시 안에서도 더 가까운 삼각형이 뒤에 나올 수 있으므로 break 하지 않는다
			NearestT = OutT;
			bHit = true;
		}
	}

	if (bHit)
	{
		OutHitT = NearestT;
	}

	return bHit;
}
