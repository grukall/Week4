
#include "CubeComponent.h"

#include "Assets.h"
#include "FAssetManager.h"

UCubeComponent::UCubeComponent()
{
	// 메시는 생성자에서 잡는다. 씬 로드는 Initialize를 거치지 않고 생성자 + DeserializeClass만
	// 타므로, Initialize에만 두면 불러온 큐브가 메시 없는 빈 컴포넌트가 된다.
	SetStaticMesh(FAssetManager::Get().GetAssetAs<UStaticMesh>(FName("CubeMesh"), true));
}

void UCubeComponent::Initialize()
{
	Initialize(FVector(0.f, 0.f, 0.f), FRotator(0.f, 0.f, 0.f), FVector(0.f, 0.f, 0.f));
}

void UCubeComponent::Initialize(FVector location, FRotator rotation, FVector scale3D)
{
	UStaticMeshComponent::Initialize(location, rotation, scale3D);
}

UCubeComponent::~UCubeComponent()
{
}
