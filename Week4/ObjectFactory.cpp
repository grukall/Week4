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

	if (!instance)
	{
		return nullptr;
	}

	try
	{
		instance->DeserializeClass(inJson);
	}
	catch (...)
	{
		// 역직렬화가 중간에 실패하면 반쯤 만들어진 객체가 GUObjectArray와 UObjectHash에 남는다.
		// 월드에는 없는데 통계와 GetObjectsOfClass에는 잡히는 유령이 되고, 로드를 다시 시도할 때마다
		// 쌓인다. 여기서 정리하고 예외는 그대로 올려보낸다 — 실패는 호출자가 알아야 한다.
		instance->Destroy();
		throw;
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
#include "AStaticMeshTestActor.h"
#include "World.h"

TMap<FString, std::function<const FClassInfo* ()>> FObjectFactory::mClassInfoMap = {
	{"UObject", &UObject::GetClass },
	{"AActor", &AActor::GetClass },
	{"AStaticMeshTestActor", &AStaticMeshTestActor::GetClass },
	{"UActorComponent", &UActorComponent::GetClass },
	{"USceneComponent", &USceneComponent::GetClass },
	{"UPrimitiveComponent", &UPrimitiveComponent::GetClass },
	{"UMeshComponent", &UMeshComponent::GetClass },
	{"UStaticMeshComponent", &UStaticMeshComponent::GetClass },
	{"UCubeComponent", &UCubeComponent::GetClass },
	{"USphereComponent", &USphereComponent::GetClass },
	{"ASpotLight", &ASpotLight::GetClass },
	{"USpotLightComponent", &USpotLightComponent::GetClass },
	{"UPlaneComponent", &UPlaneComponent::GetClass },
	{"UText3DComponent", &UText3DComponent::GetClass },
	{"UAtlasAnimationComponent", &UAtlasAnimationComponent::GetClass },
	{"UWorld", &UWorld::GetClass },
};
