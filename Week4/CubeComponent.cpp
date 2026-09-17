
#include "CubeComponent.h"

#include "Assets.h"
#include "FAssetManager.h"

UCubeComponent::UCubeComponent()
{
}

void UCubeComponent::Initialize()
{
	Initialize(FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f));
}

void UCubeComponent::Initialize(FVector location, FRotator rotation, FVector scale3D)
{
	UStaticMeshComponent::Initialize(location, rotation, scale3D);

	SetStaticMesh(FAssetManager::Get().GetAssetAs<UStaticMesh>(FName("CubeMesh"), true));
}

UCubeComponent::~UCubeComponent()
{
}
