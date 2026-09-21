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

	UMaterial* Material = nullptr;

	FMatrix WorldTransformMatrix;
	FObjectID ObejctID;

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
	bool bIsBillboard = false;

	// 복합 빌보드(3D 텍스트 등): 피벗을 중심으로 평면 전체 회전 지원
	bool bCustomPivot = false;
	FVector PivotLocation = FVector(0.f);
	FMatrix LocalTransform = FMatrix::Identity;
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

	inline void AddQuadInfo(const FRenderQuadInfo& QuadInfo, bool bScreenQuad = false)
	{
		if (bScreenQuad)
		{
			// 2D 큐에만 넣는다. 아래 월드 큐로 흘러가면 카메라 행렬로 한 번 더 그려진다.
			Quad2DInfos.Add(QuadInfo);
			return;
		}

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
		Quad2DInfos.Empty();
	}

	inline const TArray<FRenderQuadInfo>& GetOpaqueQuadInfos() const { return OpaqueQuadInfos; }
	inline const TArray<FRenderQuadInfo>& GetTransparentQuadInfos() const { return TransparentQuadInfos; }
	inline const TArray<FRenderQuadInfo>& GetOverlayQuadInfos() const { return OverlayQuadInfos; }
	inline const TArray<FRenderQuadInfo>& Get2DQuadInfos() const { return Quad2DInfos; }

private:
	TArray<FRenderQuadInfo> OpaqueQuadInfos;
	TArray<FRenderQuadInfo> TransparentQuadInfos;
	TArray<FRenderQuadInfo> OverlayQuadInfos;
	TArray<FRenderQuadInfo> Quad2DInfos;
};