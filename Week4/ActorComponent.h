#pragma once

#include "Object.h"

struct FRenderInfo;
class FRenderCollector;

class UActorComponent : public UObject
{
	REFLECT_CLASS(UActorComponent, UObject)
public:
	UActorComponent();
	virtual ~UActorComponent();

	void SetOwner(AActor* owner);
	AActor* GetOwner() const;

	// Todo: Make as pure class
	virtual void Tick(float deltaTime);
	virtual void Render(FRenderCollector& RenderCollector);
	virtual void GetRenderInfos(TArray<FRenderInfo>* outRenderInfos) const;

	// 이 컴포넌트가 마우스 픽킹 대상이면 컬렉터에 자신을 등록한다.
	// 기본은 등록하지 않는다. 충돌체가 있는 컴포넌트만 재정의한다.
	virtual void RegisterPickTarget(FRenderCollector& RenderCollector);

protected:
	AActor* mOwner;
};

