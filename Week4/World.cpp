#include "World.h"

#include <format>

#include "RenderInfo.h"
#include "JsonUtil.h"
#include "Console.h"

UWorld::~UWorld()
{
	for (AActor* removeActor : mActors)
	{
		delete removeActor;
	}
}

void UWorld::SerializeClass(json::JSON& outJson) const
{
	UObject::SerializeClass(outJson);
	json::JSON actorsJson = json::JSON::Make(json::JSON::Class::Array);

	for (const AActor* actor : mActors)
	{
		json::JSON actorJson;
		actor->SerializeClass(actorJson);
		actorsJson.append(std::move(actorJson));
	}
	outJson["Properties"]["mActors"] = actorsJson;
}

void UWorld::DeserializeClass(const json::JSON& inJson)
{
	UObject::DeserializeClass(inJson);

	const json::JSON& propertiesJson = inJson.at("Properties");

	if (!propertiesJson.hasKey("mActors") || propertiesJson.at("mActors").JSONType() != json::JSON::Class::Array)
	{
		throw std::runtime_error(std::format("{}: mActors requires an array", GetRuntimeClass()->Name));
	}

	const json::JSON& actorsJson = propertiesJson.at("mActors");

	for (const auto& actorJson : actorsJson.ArrayRange())
	{
		if (!actorJson.hasKey("ClassName") || actorJson.at("ClassName").JSONType() != json::JSON::Class::String)
		{
			throw std::runtime_error(std::format("{}: ClassName requires a string", GetRuntimeClass()->Name));
		}
		FString className(actorJson.at("ClassName").ToString());

		const FClassInfo* classInfo = FObjectFactory::GetClassInfoByName(className);
		if (!classInfo)
		{
			throw std::runtime_error(std::format("{}: Unknown class name: {}", GetRuntimeClass()->Name, className));
		}
		AActor* actor = static_cast<AActor*>(FObjectFactory::LoadObject(classInfo, actorJson));

		// AddActor의 중복 검사는 assert라 Release 빌드에서 사라진다. 씬 파일은 손으로도 고칠 수
		// 있는 외부 입력이므로, UUID가 겹치면 여기서 확실히 실패한다 — 겹친 채로 들어가면
		// UUID로 액터를 찾는 코드가 전부 엉뚱한 것을 집는다.
		if (getActorIndex(actor->UUID) != -1)
		{
			const int32 duplicatedUUID = actor->UUID;
			delete actor;
			throw std::runtime_error(std::format("{}: duplicated actor UUID: {}", GetRuntimeClass()->Name, duplicatedUUID));
		}

		AddActor(actor);
	}
}

void UWorld::PostSceneLoad(const FSceneLoadContext& context)
{
	for (AActor* actor : mActors)
	{
		actor->PostSceneLoad(context);
	}
}

void UWorld::AddActor(AActor* actor)
{
	assert(actor != nullptr);
	assert(getActorIndex(actor->UUID) == -1);

	mActors.Add(actor);
}

bool UWorld::RemoveActor(int32 actorUUID)
{
	int32 actorIndex = getActorIndex(actorUUID);
	if (actorIndex == -1)
	{
		return false;
	}

	mActors.RemoveAtSwap(actorIndex);

	return true;
}

void UWorld::Tick(float deltaTime)
{
	for (AActor* actor : mActors)
	{
		actor->Tick(deltaTime);
	}
}

void UWorld::Update(float deltaTime, FRenderCollector& outCollector)
{
	// 쿼드/라인 정보는 Render()가 그린 뒤 스스로 비운다. 월드 바깥(엔진 루프의 AABB 디버그 라인 등)에서도
	// 채워지므로 여기서 Reset 하면 남의 것까지 날린다. 메시/픽킹 배열만 여기서 갈아끼운다.
	outCollector.RenderInfos.Reset(DEFAULT_RESERVE_MEM);
	outCollector.PickTargets.Reset(DEFAULT_RESERVE_MEM);

	for (AActor* actor : mActors)
	{
		actor->Render(outCollector);
	}
}

int32 UWorld::getActorIndex(int32 actorUUID) const
{
	for (uint32 i = 0; i < mActors.Num(); ++i)
	{
		if (mActors[i]->UUID == actorUUID)
		{
			return i;
		}
	}

	return -1;
}
