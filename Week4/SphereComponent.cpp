#include "SphereComponent.h"

#include "Assets.h"
#include "FAssetManager.h"

USphereComponent::USphereComponent()
{
	// 생성자에서 잡아야 씬 로드(Initialize를 거치지 않는 경로)에서도 메시가 남는다.
	SetStaticMesh(FAssetManager::Get().GetAssetAs<UStaticMesh>(FName("SphereMesh"), true));
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
}
