#pragma once

#include "UStaticMeshComponent.h"

class UCubeComponent : public UStaticMeshComponent
{
	REFLECT_CLASS(UCubeComponent, UStaticMeshComponent)
public:
	UCubeComponent();

	void Initialize();
	void Initialize(FVector location, FRotator rotation, FVector scale3D);

	virtual ~UCubeComponent();
};
