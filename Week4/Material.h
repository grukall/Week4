#pragma once

#include "UAsset.h"
#include "Renderer.h"

class UTexture2D;
class FArchive;

class UMaterial : public UAsset
{
	REFLECT_CLASS(UMaterial, UAsset);

public:
	UMaterial() = default;
	using UAsset::Initialize;

	void Initialize(const FName& InAssetName, URenderer& InRenderer);

	virtual ~UMaterial() = default;

	virtual void SerializeClass(json::JSON& outJson) const override;
	virtual void DeserializeClass(const json::JSON& inJson) override;

	void Serialize(FArchive& Ar) override;
	void PostLoad(URenderer* Renderer) override;

	static UMaterial* DefaultMaterial;
	static void InitDefaultMaterial(URenderer* Renderer);

public:
	float GetSpecularPower() const
	{
		return SpecularPower;
	}

	void SetSpecularPower(float InValue)
	{
		SpecularPower = InValue;
	}

	float GetOpticalDensity() const
	{
		return OpticalDensity;
	}

	void SetOpticalDensity(float InValue)
	{
		OpticalDensity = InValue;
	}

	float GetTransparency() const
	{
		return Transparency;
	}

	void SetTransparency(float InValue)
	{
		Transparency = InValue;
	}

	int GetIlluminationModel() const
	{
		return IlluminationModel;
	}

	void SetIlluminationModel(int InValue)
	{
		IlluminationModel = InValue;
	}

	const FVector3& GetAmbientColor() const
	{
		return AmbientColor;
	}

	void SetAmbientColor(const FVector3& InColor)
	{
		AmbientColor = InColor;
	}

	const FVector4& GetDiffuseColor() const
	{
		return DiffuseColor;
	}

	void SetDiffuseColor(const FVector3& InColor)
	{
		DiffuseColor = FVector4(
			InColor.x,
			InColor.y,
			InColor.z,
			1.0f
		);
	}

	void SetDiffuseColor(const FVector4& InColor)
	{
		DiffuseColor = InColor;
	}

	const FVector3& GetSpecularColor() const
	{
		return SpecularColor;
	}

	void SetSpecularColor(const FVector3& InColor)
	{
		SpecularColor = InColor;
	}

	const FVector3& GetEmissiveColor() const
	{
		return EmissiveColor;
	}

	void SetEmissiveColor(const FVector3& InColor)
	{
		EmissiveColor = InColor;
	}

	const FGuid& GetAmbientTextureGuid() const
	{
		return AmbientTextureGuid;
	}

	void SetAmbientTextureGuid(const FGuid& InGuid)
	{
		AmbientTextureGuid = InGuid;
	}

	UTexture2D* GetAmbientTexture() const
	{
		return AmbientTexture;
	}

	void SetAmbientTexture(UTexture2D* InTexture)
	{
		AmbientTexture = InTexture;
	}

	bool HasAmbientTexture() const
	{
		return AmbientTexture != nullptr;
	}

	const FGuid& GetDiffuseTextureGuid() const
	{
		return DiffuseTextureGuid;
	}

	void SetDiffuseTextureGuid(const FGuid& InGuid)
	{
		DiffuseTextureGuid = InGuid;
	}

	UTexture2D* GetDiffuseTexture() const
	{
		return DiffuseTexture;
	}

	void SetDiffuseTexture(UTexture2D* InTexture)
	{
		DiffuseTexture = InTexture;
	}

	bool HasDiffuseTexture() const
	{
		return DiffuseTexture != nullptr;
	}

	const FGuid& GetSpecularTextureGuid() const
	{
		return SpecularTextureGuid;
	}

	void SetSpecularTextureGuid(const FGuid& InGuid)
	{
		SpecularTextureGuid = InGuid;
	}

	UTexture2D* GetSpecularTexture() const
	{
		return SpecularTexture;
	}

	void SetSpecularTexture(UTexture2D* InTexture)
	{
		SpecularTexture = InTexture;
	}

	bool HasSpecularTexture() const
	{
		return SpecularTexture != nullptr;
	}

	const FGuid& GetBumpTextureGuid() const
	{
		return BumpTextureGuid;
	}

	void SetBumpTextureGuid(const FGuid& InGuid)
	{
		BumpTextureGuid = InGuid;
	}

	UTexture2D* GetBumpTexture() const
	{
		return BumpTexture;
	}

	void SetBumpTexture(UTexture2D* InTexture)
	{
		BumpTexture = InTexture;
	}

	bool HasBumpTexture() const
	{
		return BumpTexture != nullptr;
	}

	const FVector2 GetUVScroll() const
	{
		return UVScroll;
	}

	void UpdateUVScroll()
	{
		UVScroll.X += UVSpeed.X;
		UVScroll.Y += UVSpeed.Y;
	}

	void SetUVSpeed(const FVector2& InSpeed)
	{
		UVSpeed = InSpeed;
		if (UVSpeed.X == 0.0f && UVSpeed.Y == 0.0f)
			UVScroll = FVector2(0.0f,0.0f);
	}

	const FVector2 GetUVSpeed() const
	{
		return UVSpeed;
	}

	const FVector3& GetTransmissionFilter() const
	{
		return TransmissionFilter;
	}

	void SetTransmissionFilter(const FVector3& InValue)
	{
		TransmissionFilter = InValue;
	}

private:
	float SpecularPower{ 32.0f };

	float OpticalDensity{ 1.0f };
	float Transparency{ 1.0f };

	int IlluminationModel{ 2 };

	FVector3 AmbientColor{ 0.2f, 0.2f, 0.2f };

	FVector4 DiffuseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	FVector3 SpecularColor{ 0.0f, 0.0f, 0.0f };
	FVector3 EmissiveColor{ 0.0f, 0.0f, 0.0f };

	FGuid AmbientTextureGuid;
	UTexture2D* AmbientTexture{ nullptr };

	FGuid DiffuseTextureGuid;
	UTexture2D* DiffuseTexture{ nullptr };

	FGuid SpecularTextureGuid;
	UTexture2D* SpecularTexture{ nullptr };

	FGuid BumpTextureGuid;
	UTexture2D* BumpTexture{ nullptr };

	FVector2 UVScroll{ 0.0f, 0.0f };
	FVector2 UVSpeed{ 0.0f, 0.0f };

	FVector TransmissionFilter{ 1.0f, 1.0f, 1.0f };
};