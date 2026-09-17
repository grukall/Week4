#pragma once

#include "UStaticMeshComponent.h"

class USphereComponent : public UStaticMeshComponent
{
	REFLECT_CLASS(USphereComponent, UStaticMeshComponent)
public:
	USphereComponent();
	virtual ~USphereComponent();

	//void Initialize(GraphicsManager* graphicsManager);
	//void Initialize(GraphicsManager* graphicsManager, FVector location, FRotator rotation, FVector scale3D);

	void Initialize();
	void Initialize(FVector location, FRotator rotation, FVector scale3D);
};
