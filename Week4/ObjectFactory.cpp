#include "ObjectFactory.h"

#include "Json/json.hpp"

#include "Actor.h"
#include "PrimitiveComponent.h"
#include "UAtlasAnimationComponent.h"
#include "UStaticMeshComponent.h"
#include "FAssetManager.h"
#include "Assets.h"
#include "UObjectHash.h"

void FObjectFactory::RegisterToHash(UObject* instance)
{
	FUObjectHashTables::Get().Add(instance);
}

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
		RegisterToHash(instance);
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

AActor* FObjectFactory::SpawnPrimitiveActor(const FName& MeshAssetName, FVector3 Location, FRotator Rotation, FVector3 Scale)
{
	// Create a new actor
	AActor* actor = ConstructObject<AActor>();

	UStaticMeshComponent* component = ConstructObject<UStaticMeshComponent>(Location, Rotation, Scale);
	component->SetStaticMesh(FAssetManager::Get().GetAssetAs<UStaticMesh>(MeshAssetName, true));

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
#include "UPlaneComponent.h"
#include "USpotLightComponent.h"
#include "ASpotLight.h"
#include "UText3DComponent.h"
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
