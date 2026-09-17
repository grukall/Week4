#include "ObjectFactory.h"

#include "Json/json.hpp"

#include "Actor.h"
#include "PrimitiveComponent.h"
#include "UAtlasAnimationComponent.h"

UObject* FObjectFactory::ConstructUnInitializedObject(const FClassInfo* classInfo)
{
	if (!classInfo || !classInfo->Constructor)
	{
		return nullptr;
	}

	UObject* instance = classInfo->CreateInstance();
	if (instance)
	{
		instance->mClassInfo = classInfo;
	}
	return instance;
}

UObject* FObjectFactory::LoadObject(const FClassInfo* classInfo, const json::JSON& inJson)
{
	UObject* instance = ConstructUnInitializedObject(classInfo);

	if (instance)
	{
		instance->DeserializeClass(inJson);
	}
	return instance;
}

AActor* FObjectFactory::SpawnPrimitiveActor(
	EPrimitive primitiveType,
	FVector3 Location, FRotator Rotation, FVector3 Scale)
{
	// Create a new actor
	AActor* actor = ConstructObject<AActor>();

	UPrimitiveComponent* component = ConstructObject<UPrimitiveComponent>(primitiveType, Location, Rotation, Scale);

	actor->AddRootSceneComponent(component);

	return actor;
}

const FClassInfo* FObjectFactory::GetClassInfoByName(const FString& className)
{
	if (!mClassInfoMap.Contains(className))
	{
		return nullptr;
	}

	return mClassInfoMap[className]();
}

bool FObjectFactory::RegisterClassInfo(FString className, const FClassInfo* classInfo)
{
	if (mClassInfoMap.Contains(className))
	{
		return false;
	}
	mClassInfoMap.Add(className, [classInfo]() -> const FClassInfo* { return classInfo; });
	return true;
}

#include "SceneComponent.h"
#include "CubeComponent.h"
#include "SphereComponent.h"
#include "UTextComponent.h"
#include "World.h"

TMap<FString, std::function<const FClassInfo* ()>> FObjectFactory::mClassInfoMap = {
	{"UObject", &UObject::GetClass },
	{"AActor", &AActor::GetClass },
	{"UActorComponent", &UActorComponent::GetClass },
	{"USceneComponent", &USceneComponent::GetClass },
	{"UPrimitiveComponent", &UPrimitiveComponent::GetClass },
	{"UCubeComponent", &UCubeComponent::GetClass },
	{"USphereComponent", &USphereComponent::GetClass },
	{"ASpotLight", &ASpotLight::GetClass },
	{"USpotLightComponent", &USpotLightComponent::GetClass },
	{"UPlaneComponent", &UPlaneComponent::GetClass },
	{"UText3DComponent", &UText3DComponent::GetClass },
	{"UAtlasAnimationComponent", &UAtlasAnimationComponent::GetClass },
	{"UWorld", &UWorld::GetClass },
};
