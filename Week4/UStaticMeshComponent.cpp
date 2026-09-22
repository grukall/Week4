#include "UStaticMeshComponent.h"

#include "Actor.h"
#include "Assets.h"
#include "Console.h"
#include "EngineMathLibrary.h"
#include "FAssetManager.h"
#include "FLogManager.h"
#include "Json/json.hpp"
#include "JsonAssetReference.h"
#include "JsonUtil.h"
#include "RenderInfo.h"

namespace
{
	// UMaterial::DefaultMaterial은 에셋 매니저에 등록되지 않은 엔진 내장 인스턴스라 이름으로도
	// GUID로도 찾을 수 없다. 표식으로 적고 로드할 때 같은 인스턴스를 돌려준다.
	json::JSON MaterialReferenceToJson(const UMaterial* Material)
	{
		if (Material != nullptr && Material == UMaterial::DefaultMaterial)
		{
			json::JSON defaultJson = json::JSON::Make(json::JSON::Class::Object);
			defaultJson["Default"] = true;
			return defaultJson;
		}

		return AssetReferenceToJson(Material);
	}
}

UStaticMeshComponent::UStaticMeshComponent()
{
}

void UStaticMeshComponent::SerializeClass(json::JSON& outJson) const
{
	UMeshComponent::SerializeClass(outJson);

	json::JSON& propertiesJson = outJson["Properties"];

	propertiesJson["StaticMesh"] = AssetReferenceToJson(StaticMesh);

	// 이 컴포넌트가 "고른" 머티리얼들. 공유 에셋이므로 참조로 적는다.
	json::JSON materialsJson = json::JSON::Make(json::JSON::Class::Array);
	for (uint32 SlotIndex = 0; SlotIndex < OverrideMaterials.Num(); ++SlotIndex)
	{
		materialsJson.append(MaterialReferenceToJson(OverrideMaterials[SlotIndex]));
	}
	propertiesJson["OverrideMaterials"] = materialsJson;

	// 이 컴포넌트에서만 값을 바꾼 전용 사본들. 디스크에 없는 인스턴스라 참조가 아니라 값으로 적는다.
	// 런타임에 바뀔 수 있는 값만 담으면 된다 — 나머지는 로드할 때 원본 머티리얼에서 다시 복사된다.
	json::JSON instancesJson = json::JSON::Make(json::JSON::Class::Array);
	for (uint32 SlotIndex = 0; SlotIndex < InstancedMaterials.Num(); ++SlotIndex)
	{
		const UMaterial* Instance = InstancedMaterials[SlotIndex];
		if (Instance == nullptr)
		{
			continue;
		}

		json::JSON instanceJson = json::JSON::Make(json::JSON::Class::Object);
		instanceJson["Slot"] = static_cast<int32>(SlotIndex);
		instanceJson["Texture"] = AssetReferenceToJson(Instance->GetDiffuseTexture());

		const FVector2 UVSpeed = Instance->GetUVSpeed();
		json::JSON uvSpeedJson = json::JSON::Make(json::JSON::Class::Array);
		uvSpeedJson[0] = static_cast<float>(UVSpeed.X);
		uvSpeedJson[1] = static_cast<float>(UVSpeed.Y);
		instanceJson["UVSpeed"] = uvSpeedJson;

		instancesJson.append(instanceJson);
	}
	propertiesJson["MaterialInstances"] = instancesJson;
}

void UStaticMeshComponent::DeserializeClass(const json::JSON& inJson)
{
	UMeshComponent::DeserializeClass(inJson);

	const json::JSON& propertiesJson = inJson.at("Properties");

	// 메시 참조가 없는 옛 씬 파일은 생성자가 잡아둔 기본 메시를 그대로 둔다.
	if (propertiesJson.hasKey("StaticMesh"))
	{
		const json::JSON& meshJson = propertiesJson.at("StaticMesh");

		if (meshJson.JSONType() == json::JSON::Class::Null)
		{
			SetStaticMesh(nullptr);
		}
		else if (UStaticMesh* Mesh = AssetReferenceFromJson<UStaticMesh>(meshJson))
		{
			SetStaticMesh(Mesh);
		}
		else
		{
			// 에셋을 못 찾았다고 기본 메시까지 날리면 씬에서 물체가 통째로 사라진다.
			UE_LOG_WARN("%s: failed to resolve StaticMesh reference, keeping the current mesh",
				GetRuntimeClass()->Name.CStr());
		}
	}

	// SetStaticMesh가 OverrideMaterials를 메시 기본값으로 덮어쓰므로 반드시 그 뒤에 적용한다.
	if (propertiesJson.hasKey("OverrideMaterials")
		&& propertiesJson.at("OverrideMaterials").JSONType() == json::JSON::Class::Array)
	{
		const json::JSON& materialsJson = propertiesJson.at("OverrideMaterials");
		const int32 SlotCount = materialsJson.length();

		for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
		{
			const json::JSON& materialJson = materialsJson.at(static_cast<unsigned>(SlotIndex));

			if (materialJson.JSONType() == json::JSON::Class::Null)
			{
				SetMaterial(static_cast<uint32>(SlotIndex), nullptr);
				continue;
			}

			if (materialJson.JSONType() == json::JSON::Class::Object && materialJson.hasKey("Default"))
			{
				SetMaterial(static_cast<uint32>(SlotIndex), UMaterial::DefaultMaterial);
				continue;
			}

			if (UMaterial* Material = AssetReferenceFromJson<UMaterial>(materialJson))
			{
				SetMaterial(static_cast<uint32>(SlotIndex), Material);
				continue;
			}

			// 못 찾은 슬롯은 건드리지 않는다. 메시가 준 기본 머티리얼이 남아 있는 편이 낫다.
			UE_LOG_WARN("%s: failed to resolve material reference for slot %d",
				GetRuntimeClass()->Name.CStr(), SlotIndex);
		}
	}

	// 전용 사본은 슬롯 선택이 끝난 뒤에 만든다. SetMaterial이 그 슬롯의 사본을 버리기 때문이다.
	if (propertiesJson.hasKey("MaterialInstances")
		&& propertiesJson.at("MaterialInstances").JSONType() == json::JSON::Class::Array)
	{
		const json::JSON& instancesJson = propertiesJson.at("MaterialInstances");
		const int32 InstanceCount = instancesJson.length();

		for (int32 i = 0; i < InstanceCount; ++i)
		{
			const json::JSON& instanceJson = instancesJson.at(static_cast<unsigned>(i));
			if (instanceJson.JSONType() != json::JSON::Class::Object)
			{
				continue;
			}

			int32 SlotIndex = -1;
			ReadJsonInt(instanceJson, "Slot", SlotIndex);
			if (SlotIndex < 0)
			{
				UE_LOG_WARN("%s: material instance without a valid slot index", GetRuntimeClass()->Name.CStr());
				continue;
			}

			UMaterial* Instance = GetMaterialForEdit(static_cast<uint32>(SlotIndex));
			if (Instance == nullptr)
			{
				continue;
			}

			if (instanceJson.hasKey("Texture"))
			{
				// 텍스처를 못 찾으면 원본에서 복사된 값을 그대로 둔다. 비우면 물체가 검게 나온다.
				const json::JSON& textureJson = instanceJson.at("Texture");
				if (textureJson.JSONType() == json::JSON::Class::Null)
				{
					Instance->SetDiffuseTexture(nullptr);
				}
				else if (UTexture2D* Texture = AssetReferenceFromJson<UTexture2D>(textureJson))
				{
					Instance->SetDiffuseTexture(Texture);
				}
				else
				{
					UE_LOG_WARN("%s: failed to resolve instanced material texture for slot %d",
						GetRuntimeClass()->Name.CStr(), SlotIndex);
				}
			}

			if (instanceJson.hasKey("UVSpeed") && IsJsonNumberArray(instanceJson.at("UVSpeed"), 2))
			{
				const json::JSON& uvSpeedJson = instanceJson.at("UVSpeed");
				Instance->SetUVSpeed(FVector2(
					static_cast<float>(uvSpeedJson.at(0u).ToFloat()),
					static_cast<float>(uvSpeedJson.at(1u).ToFloat())));
			}
		}
	}
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

	// 메시가 바뀌면 슬롯 구성 자체가 달라진다. 이전 메시의 슬롯에 맞춰 만든 사본도 같이 버린다.
	releaseAllInstancedMaterials();
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
	// 슬롯 대입과 사본 정리를 한 군데에서만 하도록 부모에게 맡긴다.
	// 여기서 따로 구현하면 사본이 남아 "고른 머티리얼"과 "보이는 머티리얼"이 어긋난다.
	UMeshComponent::SetMaterial(MaterialSlotIndex, InMaterial);
}

UMaterial* UStaticMeshComponent::GetMaterial(uint32 MaterialSlotIndex) const
{
	// 전용 사본 -> 이 컴포넌트가 고른 머티리얼 -> 메시 에셋의 기본 머티리얼 순.
	if (UMaterial* Instanced = GetInstancedMaterial(MaterialSlotIndex))
	{
		return Instanced;
	}

	if (OverrideMaterials.Num() > MaterialSlotIndex) {
		return OverrideMaterials[MaterialSlotIndex];
	}

	if (!StaticMesh)	return nullptr;

	return StaticMesh->GetMaterial(MaterialSlotIndex);
}

void UStaticMeshComponent::ClearMaterials()
{
	releaseAllInstancedMaterials();
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
