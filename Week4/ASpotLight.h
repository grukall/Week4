#pragma once

#include "Actor.h"
#include "ObjectFactory.h"
#include "UPlaneComponent.h"
#include "USpotLightComponent.h"
#include "Json/json.hpp"

class ASpotLight : public AActor
{
	REFLECT_CLASS(ASpotLight, AActor)

public:
	ASpotLight() = default;
	void Initialize()
	{
		Super::Initialize();
		USpotLightComponent* SpotLightComponent = FObjectFactory::ConstructObject<USpotLightComponent>(FVector(0, 0, 0), FRotator(0, 0, 0), FVector(1, 1, 1));
		AddRootSceneComponent(SpotLightComponent);
	}

	void DeserializeClass(const json::JSON& inJson) override
	{
		Super::DeserializeClass(inJson);

		for (UActorComponent* Component : GetComponents())
		{
			UPlaneComponent* PlaneComponent = Component->Cast<UPlaneComponent>();

			if (!PlaneComponent)
			{
				continue;
			}

			// SetTexture는 필요 없음

			PlaneComponent->SetBlendState(ERenderBlendMode::Transparent);
			PlaneComponent->SetBillboard(true);
			PlaneComponent->SetDepthState(true, false);
		}
	}

	void RestoreRuntimeCamera(FCamera& Camera)
	{
		for (UActorComponent* Component : GetComponents())
		{
			UPlaneComponent* PlaneComponent =
				Component->Cast<UPlaneComponent>();

			if (!PlaneComponent)
			{
				continue;
			}

			PlaneComponent->SetBillboardCamera(
				Camera
			);
		}
	}
};
