#include "SphereComponent.h"

#include "Assets.h"
#include "FAssetManager.h"

USphereComponent::USphereComponent()
{
}

USphereComponent::~USphereComponent()
{
}

void USphereComponent::Initialize()
{
	Initialize(FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f));
}

void USphereComponent::Initialize(FVector location, FRotator rotation, FVector scale3D)
{
	UStaticMeshComponent::Initialize(location, rotation, scale3D);

	SetStaticMesh(FAssetManager::Get().GetAssetAs<UStaticMesh>(FName("SphereMesh"), true));
}
