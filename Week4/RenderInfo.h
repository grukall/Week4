#pragma once

#include "Transform.h"
#include "Object.h"
#include "FName.h"
#include "Assets.h"
#include "TArray.h"

class FCamera;
class UPrimitiveComponent;
class UMaterial;
enum class ERenderBlendMode
{
	Opaque,
	Masked,
	Transparent,
	Additive,
	NoColorWrite,
	Count
};

struct FRenderInfo
{
	UStaticMesh* StaticMesh = nullptr;

	// UMaterial이 들어오면 이 두 개가 머티리얼 포인터로 합쳐진다.
	/*UTexture2D* Texture = nullptr;
	FVector4 Color = FVector4(1.f, 1.f, 1.f, 1.f);*/

	UMaterial* Material = nullptr;

	FMatrix WorldTransformMatrix;
	FObjectID ObejctID;

	// StaticMesh->GetSections()의 인덱스. 이 구간만 그린다.
	uint32 SectionIndex = 0;
};

struct FRenderQuadInfo
{
	FMatrix Model;
	FVector4 Color = { 1.f, 1.f, 1.f, 1.f };
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> TextureSRV;
	FVector4 SubUV = { 0.f, 0.f, 1.f, 1.f };
	ERenderBlendMode BlendMode = ERenderBlendMode::Opaque;
	bool EnableDepthTest = true;
	bool EnableDepthWrite = true;
};

struct FRenderLineInfo
{
	FVector4 Color;
	FVector3 Start;
	float Thickness;
	FVector3 End;
	float Padding;
};

// 이번 프레임에 그릴 것들을 한데 모은다. 소유자는 FGraphicsManager.
struct FRenderCollector
{
public:
	enum { DEFAULT_RESERVE_MEM = 1024U };

	FCamera* Camera = nullptr;

	TArray<FRenderInfo>     RenderInfos;   // 메시 패스
	TArray<FRenderLineInfo> LineInfos;     // 라인 패스
	TArray<UPrimitiveComponent*> PickTargets;

	inline void AddQuadInfo(const FRenderQuadInfo& QuadInfo)
	{
		if (QuadInfo.EnableDepthTest)
		{
			if (QuadInfo.EnableDepthWrite)
			{
				OpaqueQuadInfos.Add(QuadInfo);
			}
			else
			{
				TransparentQuadInfos.Add(QuadInfo);
			}
		}
		else
		{
			OverlayQuadInfos.Add(QuadInfo);
		}
	}

	inline void Clear()
	{
		RenderInfos.Empty();
		LineInfos.Empty();
		PickTargets.Empty();
		OpaqueQuadInfos.Empty();
		TransparentQuadInfos.Empty();
		OverlayQuadInfos.Empty();
	}

	inline const TArray<FRenderQuadInfo>& GetOpaqueQuadInfos() const { return OpaqueQuadInfos; }
	inline const TArray<FRenderQuadInfo>& GetTransparentQuadInfos() const { return TransparentQuadInfos; }
	inline const TArray<FRenderQuadInfo>& GetOverlayQuadInfos() const { return OverlayQuadInfos; }

private:
	TArray<FRenderQuadInfo> OpaqueQuadInfos;
	TArray<FRenderQuadInfo> TransparentQuadInfos;
	TArray<FRenderQuadInfo> OverlayQuadInfos;
};