#pragma once

#include "Actor.h"

class UStaticMeshComponent;

class AStaticMeshTestActor : public AActor
{
	REFLECT_CLASS(AStaticMeshTestActor, AActor)

public:
	AStaticMeshTestActor() = default;
	void Initialize();

	UStaticMeshComponent* StaticMeshComponent = nullptr;
};