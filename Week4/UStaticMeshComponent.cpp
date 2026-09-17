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

UTexture2D* UStaticMeshComponent::GetRenderTexture() const
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

	UTexture2D* RenderTexture = GetRenderTexture();
	const FVector4 RenderColor = GetRenderColor();

	// 섹션마다 따로 넘긴다. 나중에 머티리얼 기준으로 정렬하거나 묶을 여지를 남긴다.
	for (uint32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		FRenderInfo RenderInfo;
		RenderInfo.StaticMesh = StaticMesh;
		RenderInfo.Texture = RenderTexture;
		RenderInfo.WorldTransformMatrix = WorldMatrix;
		RenderInfo.ObejctID = ObjectID;
		RenderInfo.Color = RenderColor;
		RenderInfo.SectionIndex = SectionIndex;

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
