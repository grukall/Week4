#include "AStaticMeshTestActor.h"
#include "UStaticMeshComponent.h"
#include "ObjectFactory.h"

void AStaticMeshTestActor::Initialize()
{
	Super::Initialize();

	StaticMeshComponent = FObjectFactory::ConstructObject<UStaticMeshComponent>();
	AddRootSceneComponent(StaticMeshComponent);
}
