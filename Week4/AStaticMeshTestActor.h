#pragma once

#include "Actor.h"

class UStaticMeshComponent;

class AStaticMeshTestActor : public AActor
{
	REFLECT_CLASS(AStaticMeshTestActor, AActor)

public:
	AStaticMeshTestActor() = default;
	void Initialize();

	// 로드 경로는 Initialize를 타지 않으므로, 파일에서 복원된 루트 컴포넌트로 다시 묶어준다.
	virtual void DeserializeClass(const json::JSON& inJson) override;

	UStaticMeshComponent* StaticMeshComponent = nullptr;
};