#pragma once

#include "Object.h"

class UTexture2D;
class UMaterial : public UObject {
	REFLECT_CLASS(UMaterial, UObject)
public:
	UMaterial();
	virtual ~UMaterial() = default;

	virtual void SerializeClass(json::JSON& outJson) const override;
	virtual void DeserializeClass(const json::JSON& inJson) override;

public:
	const FVector4& GetDiffuseColor() const {
		return DiffuseColor;
	}

	void SetDiffuseColor(const FVector4 InColor) {
		DiffuseColor = InColor;
	}

	//Diffuse Texture
	const FString& GetDiffuseTexturePath() const {
		return DiffuseTexturePath;
	}

	void SetDiffuseTexturePath(const FString& InPath) {
		DiffuseTexturePath = InPath;
	}

	UTexture2D* GetDiffuseTexture() const {
		return DiffuseTexture;
	}

	void SetDiffuseTexture(UTexture2D* InTexture) {
		DiffuseTexture = InTexture;
	}

	bool HasDiffuseTexture() const {
		return DiffuseTexture != nullptr;
	}

private:
	FVector4 DiffuseColor;
	FString DiffuseTexturePath;
	UTexture2D* DiffuseTexture;
};