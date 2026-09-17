
#include "PrimitiveComponent.h"

#include <format>

#include "RenderInfo.h"
#include "enum.h"
#include "JsonUtil.h"
#include "Console.h"
#include "Actor.h"
#include "FAssetManager.h"
#include "EngineMathLibrary.h"

#include "Cube.h"
#include "Sphere.h"
#include "Triangle.h"
#include "GizmoArrow.h"
#include "Circle.h"
#include "Plane.h"
#include "ShowFlags.h"

// 정점 배열이 보이는 스코프라 sizeof 로 개수가 나온다.
// 포인터로 받으면 배열 크기 정보가 사라지므로 여기서 개수를 같이 넘긴다.
static bool GetPrimitiveMesh(EPrimitive ePrimitive, const FVertexSimple*& OutVertices, uint32& OutCount, const uint32*& OutIndices, uint32& OutIndexCount)
{
	switch (ePrimitive)
	{
	case EPrimitive::EP_Cube:
		OutVertices = Cube_vertices;
		OutCount = static_cast<uint32>(sizeof(Cube_vertices) / sizeof(FVertexSimple));
		OutIndices = Cube_indices;
		OutIndexCount = static_cast<uint32>(sizeof(Cube_indices) / sizeof(uint32));
		return true;
	case EPrimitive::EP_Sphere:
		OutVertices = Sphere_vertices;
		OutCount = static_cast<uint32>(sizeof(Sphere_vertices) / sizeof(FVertexSimple));
		OutIndices = Sphere_indices;
		OutIndexCount = static_cast<uint32>(sizeof(Sphere_indices) / sizeof(uint32));
		return true;
	case EPrimitive::EP_Triangle:
		OutVertices = Triangle_vertices;
		OutCount = static_cast<uint32>(sizeof(Triangle_vertices) / sizeof(FVertexSimple));
		OutIndices = Triangle_indices;
		OutIndexCount = static_cast<uint32>(sizeof(Triangle_indices) / sizeof(uint32));
		return true;
	case EPrimitive::EP_GizmoArrow:
		OutVertices = GizmoArrow_vertices;
		OutCount = static_cast<uint32>(sizeof(GizmoArrow_vertices) / sizeof(FVertexSimple));
		OutIndices = GizmoArrow_indices;
		OutIndexCount = static_cast<uint32>(sizeof(GizmoArrow_indices) / sizeof(uint32));
		return true;
	case EPrimitive::EP_Circle:
		OutVertices = Circle_vertices;
		OutCount = static_cast<uint32>(sizeof(Circle_vertices) / sizeof(FVertexSimple));
		OutIndices = Circle_indices;
		OutIndexCount = static_cast<uint32>(sizeof(Circle_indices) / sizeof(uint32));
		return true;
	case EPrimitive::EP_Plane:
		OutVertices = Plane_vertices;
		OutCount = static_cast<uint32>(sizeof(Plane_vertices) / sizeof(FVertexSimple));
		OutIndices = Plane_indices;
		OutIndexCount = static_cast<uint32>(sizeof(Plane_indices) / sizeof(uint32));
		return true;
	}

	return false;
}

UPrimitiveComponent::UPrimitiveComponent()
{
}

/*
void UPrimitiveComponent::Initialize(GraphicsManager* graphicsManager, EPrimitive ePrimitive, FVector location, FRotator rotation, FVector scale3D)
{
	USceneComponent::Initialize(location, rotation, scale3D);

	mGraphicsManager = graphicsManager;
	mePrimitive = ePrimitive;
}
*/

void UPrimitiveComponent::Initialize(EPrimitive ePrimitive)
{
	Initialize(ePrimitive, FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f));
}

void UPrimitiveComponent::Initialize(EPrimitive ePrimitive, FVector location, FRotator rotation, FVector scale3D)
{
	USceneComponent::Initialize(location, rotation, scale3D);

	mePrimitive = ePrimitive;

	FName MeshAssetName;
	switch (mePrimitive)
	{
		case EPrimitive::EP_Sphere:		MeshAssetName = "SphereMesh"; break;
		case EPrimitive::EP_Cube:		MeshAssetName = "CubeMesh"; break;
		case EPrimitive::EP_Triangle:	MeshAssetName = "TriangleMesh"; break;
		case EPrimitive::EP_GizmoArrow:	MeshAssetName = "GizmoArrowMesh"; break;
		case EPrimitive::EP_Circle:		MeshAssetName = "CircleMesh"; break;
		case EPrimitive::EP_Plane:		MeshAssetName = "PlaneMesh"; break;
	}

	mMeshAsset = FAssetManager::Get().GetAssetAs<FStaticMeshAsset>(MeshAssetName);
}

UPrimitiveComponent::~UPrimitiveComponent()
{
}

void UPrimitiveComponent::SerializeClass(json::JSON& outJson) const
{
	USceneComponent::SerializeClass(outJson);
	outJson["Properties"]["mePrimitiveType"] = EPrimitiveToJson(mePrimitive);
	outJson["Properties"]["TextureAssetName"] = mTextureAsset ? mTextureAsset->GetAssetName().ToString().CStr() : "";
}

void UPrimitiveComponent::DeserializeClass(const json::JSON& inJson)
{
	USceneComponent::DeserializeClass(inJson);

	const json::JSON& propertiesJson = inJson.at("Properties");
	if (!propertiesJson.hasKey("mePrimitiveType") || propertiesJson.at("mePrimitiveType").JSONType() != json::JSON::Class::String)
	{
		throw std::runtime_error(std::format("{}: mePrimitiveType property requires a string", GetRuntimeClass()->Name));
	}

	mePrimitive = EPrimitiveFromJson(propertiesJson.at("mePrimitiveType"));

	RestoreMeshAsset();


	if (propertiesJson.hasKey("TextureAssetName") && propertiesJson.at("TextureAssetName").JSONType()
		== json::JSON::Class::String)
	{
		const FString AssetName = propertiesJson.at("TextureAssetName").ToString();

		if (AssetName.Len() > 0)
		{
			mTextureAsset = FAssetManager::Get().GetAssetAs<FTexture2DAsset>(FName(AssetName), true);
		}
		else
		{
			mTextureAsset = nullptr;
		}
	}
}

void UPrimitiveComponent::RestoreMeshAsset()
{
	FName MeshAssetName;

	switch (mePrimitive)
	{
	case EPrimitive::EP_Sphere:
		MeshAssetName = "SphereMesh";
		break;

	case EPrimitive::EP_Cube:
		MeshAssetName = "CubeMesh";
		break;

	case EPrimitive::EP_Triangle:
		MeshAssetName = "TriangleMesh";
		break;

	case EPrimitive::EP_GizmoArrow:
		MeshAssetName = "GizmoArrowMesh";
		break;

	case EPrimitive::EP_Circle:
		MeshAssetName = "CircleMesh";
		break;

	case EPrimitive::EP_Plane:
		MeshAssetName = "PlaneMesh";
		break;
	}

	mMeshAsset = FAssetManager::Get().GetAssetAs<FStaticMeshAsset>(MeshAssetName, true);
}


void UPrimitiveComponent::Render(FRenderCollector& RenderCollector)
{
	if (FShowFlags::Get().IsEnabled(EShowFlag::Primitive))
		RenderCollector.RenderInfos.Add({ mMeshAsset, mTextureAsset, mePrimitive, GetTransformMatrix().MakeMatrix(),{ mOwner->UUID, mOwner->InternalIndex }, FVector4(0, 0, 0, 0) });
}

void UPrimitiveComponent::GetRenderInfos(TArray<FRenderInfo>* outRenderInfos) const
{
	assert(outRenderInfos);

	outRenderInfos->Add({ mMeshAsset, mTextureAsset, mePrimitive, GetTransformMatrix().MakeMatrix(),{mOwner->UUID, mOwner->InternalIndex}, FVector4(0, 0, 0, 0)});
}

void UPrimitiveComponent::RegisterPickTarget(FRenderCollector& RenderCollector)
{
	RenderCollector.PickTargets.Add(this);
}

bool UPrimitiveComponent::RayCastComponent(const FPickingRay& PickingRay, float& OutHitT) const
{
	if (!mMeshAsset)
	{
		return false;
	}

	const FMatrix WorldMatrix = GetTransformMatrix().MakeMatrix();

	// AABB 충돌체를 이용한 광선-메시 충돌 최적화
	const FAABB BoundingBox = mMeshAsset->GetLocalBoundingBox().ToWorld(WorldMatrix);
	if (!RayIntersectsAABB(PickingRay.ToRay(), PickingRay.Length, BoundingBox))
	{
		return false;
	}

	// 메시 충돌체를 이용한 광선-삼각형 충돌 판정
	const FVertexSimple* vertices = nullptr;
	uint32 length = 0;
	const uint32* indices = nullptr;
	uint32 indexCount = 0;
	if (!GetPrimitiveMesh(mePrimitive, vertices, length, indices, indexCount))
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
	for (int32 i = 0; i < indexCount; i += 3)
	{
		const FVector V0 = vertices[indices[i]].GetPosition();
		const FVector V1 = vertices[indices[i + 1]].GetPosition();
		const FVector V2 = vertices[indices[i + 2]].GetPosition();

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


/*
void UPrimitiveComponent::Render(FStruct)
{
	// Todo: Fix renderer
	mGraphicsManager->Render(GetTransformMatrix(), mePrimitive);
}
*/


