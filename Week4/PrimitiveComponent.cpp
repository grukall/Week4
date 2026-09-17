
#include "PrimitiveComponent.h"

#include <format>

#include "RenderInfo.h"
#include "enum.h"
#include "JsonUtil.h"
#include "Console.h"
#include "Actor.h"
#include "FAssetManager.h"
#include "EngineMathLibrary.h"

#include "ShowFlags.h"


UPrimitiveComponent::UPrimitiveComponent()
{
}


void UPrimitiveComponent::Initialize()
{
	Initialize(FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f));
}

void UPrimitiveComponent::Initialize(FVector location, FRotator rotation, FVector scale3D)
{
	USceneComponent::Initialize(location, rotation, scale3D);
}

UPrimitiveComponent::~UPrimitiveComponent()
{
}

void UPrimitiveComponent::SerializeClass(json::JSON& outJson) const
{
	USceneComponent::SerializeClass(outJson);
}

void UPrimitiveComponent::DeserializeClass(const json::JSON& inJson)
{
	USceneComponent::DeserializeClass(inJson);
}

void UPrimitiveComponent::Render(FRenderCollector& RenderCollector)
{
	if (FShowFlags::Get().IsEnabled(EShowFlag::Primitive))
	{
		GetRenderInfos(&RenderCollector.RenderInfos);
	}
}

void UPrimitiveComponent::GetRenderInfos(TArray<FRenderInfo>* outRenderInfos) const
{
	assert(outRenderInfos);

	// 이 단계에는 그릴 지오메트리가 없다. 파생 클래스가 채운다.
}

void UPrimitiveComponent::RegisterPickTarget(FRenderCollector& RenderCollector)
{
	RenderCollector.PickTargets.Add(this);
}

bool UPrimitiveComponent::RayCastComponent(const FPickingRay& PickingRay, float& OutHitT) const
{
	return false;
}