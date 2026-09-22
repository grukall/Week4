#include "AStaticMeshTestActor.h"
#include "UStaticMeshComponent.h"
#include "ObjectFactory.h"

void AStaticMeshTestActor::Initialize()
{
	Super::Initialize();

	StaticMeshComponent = FObjectFactory::ConstructObject<UStaticMeshComponent>();
	AddRootSceneComponent(StaticMeshComponent);
}

void AStaticMeshTestActor::DeserializeClass(const json::JSON& inJson)
{
	Super::DeserializeClass(inJson);

	USceneComponent* RootComponent = GetRootComponent();
	StaticMeshComponent = RootComponent ? RootComponent->Cast<UStaticMeshComponent>() : nullptr;
}
