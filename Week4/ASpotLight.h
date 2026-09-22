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

			// 텍스처를 저장하지 않던 옛 씬 파일에는 아이콘이 비어 있다. 그때는 다시 물려준다.
			// (예전에는 SetTexture가 공유 머티리얼을 덮어써서 아무 데서나 아이콘이 따라붙었다.)
			if (!PlaneComponent->GetTexture())
			{
				PlaneComponent->SetTexture(
					FAssetManager::Get().GetAssetAs<UTexture2D>(FName("SpotLightIcon"), true));
			}

			// 아이콘 표현은 이 액터가 정하는 고정값이다. 플레인이 상태를 직렬화하게 된 지금은
			// 새 씬 파일에서 같은 값이 그대로 올라오고, 이 코드는 옛 파일을 위한 보정으로 남는다.
			PlaneComponent->SetBlendState(ERenderBlendMode::Transparent);
			PlaneComponent->SetBillboard(true);
			PlaneComponent->SetDepthState(true, false);
		}
	}

};
