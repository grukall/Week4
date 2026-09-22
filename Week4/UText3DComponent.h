#pragma once

#include "SceneComponent.h"
#include "Assets.h"
#include "Camera.h"
#include "Actor.h"
#include "FAssetManager.h"
#include "ShowFlags.h"
#include "MathUtility.h"
#include "Json/json.hpp"
#include "JsonAssetReference.h"
#include "JsonUtil.h"
#include "Property.h"
#include "UStaticMeshComponent.h"


class UText3DComponent : public USceneComponent
{
	REFLECT_CLASS(UText3DComponent, USceneComponent)
	REFLECT_START(className)
		//PROPERTY(mFontAtlasAsset) // @error
		PROPERTY(mColor)
		PROPERTY(mbBillboard)
		PROPERTY(mEnableDepthTest)
		PROPERTY(mEnableDepthWrite)
	REFLECT_END()
public:
	UText3DComponent() = default;

	void SerializeClass(json::JSON& outJson) const override
	{
		Super::SerializeClass(outJson);

		json::JSON& propertiesJson = outJson["Properties"];

		// std::wstring을 UTF-8 문자열로 변환하여 저장
		propertiesJson["mText"] = Wide2Utf(mText).CStr();

		propertiesJson["mFontAtlasAsset"] = AssetReferenceToJson(mFontAtlasAsset);
		propertiesJson["mColor"] = FVector4ToJson(mColor);
		propertiesJson["mbBillboard"] = mbBillboard;
		propertiesJson["mEnableDepthTest"] = mEnableDepthTest;
		propertiesJson["mEnableDepthWrite"] = mEnableDepthWrite;
	}

	void DeserializeClass(const json::JSON& inJson) override
	{
		Super::DeserializeClass(inJson);

		const json::JSON& propertiesJson =
			inJson.at("Properties");

		// 이전 버전 씬 파일과의 호환성을 위해 필수가 아닌 값으로 처리
		if (propertiesJson.hasKey("mText") &&
			propertiesJson.at("mText").JSONType() ==
			json::JSON::Class::String)
		{
			mText = Utf2Wide(
				FString(propertiesJson.at("mText").ToString())
			);
		}
		else
		{
			mText.clear();
		}

		ReadJsonAssetReference(propertiesJson, "mFontAtlasAsset", mFontAtlasAsset);
		ReadJsonVector4(propertiesJson, "mColor", mColor);
		ReadJsonBool(propertiesJson, "mbBillboard", mbBillboard);
		ReadJsonBool(propertiesJson, "mEnableDepthTest", mEnableDepthTest);
		ReadJsonBool(propertiesJson, "mEnableDepthWrite", mEnableDepthWrite);
	}

	void PostSceneLoad(const FSceneLoadContext& context) override
	{
		Super::PostSceneLoad(context);

		// 카메라는 파일에 담기지 않는다.
		if (context.Camera)
		{
			SetBillboardCamera(*context.Camera);
		}

		// 폰트 아틀라스 참조가 없는 옛 씬 파일은 기본 아틀라스로 되살린다.
		if (!mFontAtlasAsset)
		{
			SetFontAtlasAsset(
				FAssetManager::Get().GetAssetAs<UFontAtlas>(
					FName("TestFontAtlas"),
					true
				)
			);
		}

		// mText를 저장하지 않던 씬 파일이 있다. 빈 텍스트면 UUID 문구를 다시 만든다.
		if (mText.empty() && mOwner)
		{
			SetText(Utf2Wide(FString(std::format("UUID: {}", mOwner->UUID))));
		}
	}

	FVector GetWorldPivotLocation() const
	{
		if (!mOwner)
		{
			return GetRelativeLocation();
		}

		const FTransform ParentTransform = mOwner->GetTransform();

		for (UActorComponent* Component : mOwner->GetComponents())
		{
			if (UStaticMeshComponent* MeshComp = Component->Cast<UStaticMeshComponent>())
			{
				if (UStaticMesh* Mesh = MeshComp->GetStaticMesh())
				{
					const FAABB& LocalBox = Mesh->GetLocalBoundingBox();
					if (LocalBox.Max.z > LocalBox.Min.z)
					{
						const FMatrix WorldMatrix = ParentTransform.MakeMatrix();
						const FAABB WorldBox = LocalBox.ToWorld(WorldMatrix);
						const FVector Center = (WorldBox.Min + WorldBox.Max) * 0.5f;
						return FVector(Center.x, Center.y, WorldBox.Max.z + 0.3f);
					}
				}
			}
		}

		return ParentTransform.Location + FVector(0.f, 0.f, 1.2f);
	}

	void Tick(float DeltaTime) override
	{
		if (mOwner)
		{
			SetRelativeLocation(GetWorldPivotLocation());
		}
	}

	void Render(FRenderCollector& RenderCollector) override
	{
		// Show Flags에서 끄면 쿼드를 아예 만들지 않는다.
		if (!FShowFlags::Get().IsEnabled(EShowFlag::UUIDText))
		{
			return;
		}

		if (mText.empty() && mOwner)
		{
			mText = Utf2Wide(std::format("UUID: {}", mOwner->UUID));
		}

		if (!mFontAtlasAsset)
		{
			return;
		}

		FFontAtlas* fontAtlas = mFontAtlasAsset->GetFontAtlas();
		if (!fontAtlas)
		{
			return;
		}

		// Calculate the total size of the text in world units
		const float WorldLineHeight = fontAtlas->LineHeight() * WorldUnitPerPixel;
		const float WorldAscender = fontAtlas->Ascender() * WorldUnitPerPixel;
		const float WorldDescender = fontAtlas->Descender() * WorldUnitPerPixel;

		const FVector CameraForward = RenderCollector.Camera->GetForwardVector();
		const FVector CameraRight = RenderCollector.Camera->GetRightVector();
		const FVector CameraUp = RenderCollector.Camera->GetUpVector();

		float TotalWidth = 0.0f;
		float TotalHeight = 0.0f;
		uint32 LineCount = 1;

		float CurrentLineWidth = 0.0f;
		for (wchar_t C : mText)
		{
			if (C == L'\n')
			{
				TotalWidth = FPlatformMath::Max(TotalWidth, CurrentLineWidth);
				LineCount++;
				CurrentLineWidth = 0.0f;
				continue;
			}

			if (!fontAtlas->HasGlyph(C))
			{
				fontAtlas->AddGlyph(C);
			}

			const FFontGlyph& Glyph = fontAtlas->GetGlyph(C);

			float WorldAdvanceX = Glyph.AdvanceX * WorldUnitPerPixel;

			CurrentLineWidth += WorldAdvanceX;
		}
		TotalWidth = FPlatformMath::Max(TotalWidth, CurrentLineWidth);
		TotalHeight = (WorldAscender - WorldDescender) + (LineCount - 1) * WorldLineHeight;

		// Append the text quads to the output array
		FTransform PivotTransform = GetTransformMatrix();
		if (mOwner)
		{
			PivotTransform.Location = GetWorldPivotLocation();
		}

		FVector TextLocation = FVector(0.f, -TotalWidth * 0.5f, TotalHeight * 0.5f - WorldAscender);
		for (wchar_t C : mText)
		{
			if (C == L'\n')
			{
				TextLocation.y = -TotalWidth * 0.5f;
				TextLocation.z -= WorldLineHeight;
				continue;
			}

			if (!fontAtlas->HasGlyph(C))
			{
				continue;
			}

			const FFontGlyph& Glyph = fontAtlas->GetGlyph(C);

			float WorldWidth = Glyph.Width * WorldUnitPerPixel;
			float WorldHeight = Glyph.Height * WorldUnitPerPixel;
			float WorldAdvance = Glyph.AdvanceX * WorldUnitPerPixel;
			float WorldBearingX = Glyph.BearingX * WorldUnitPerPixel;
			float WorldBearingY = Glyph.BearingY * WorldUnitPerPixel;

			FVector GlyphCenter(TextLocation.x, TextLocation.y + WorldBearingX + WorldWidth * 0.5f, TextLocation.z + WorldBearingY - WorldHeight * 0.5f);
			FMatrix TextLocalModel = FMatrix::Scale(FVector3(1.0f, WorldWidth, WorldHeight)) * FMatrix::Translation(GlyphCenter);
			TextLocalModel *= FMatrix::Scale(PivotTransform.Scale);

			FMatrix TextModel = TextLocalModel * FMatrix::Rotate(PivotTransform.Rotation) * FMatrix::Translation(PivotTransform.Location);

			FRenderQuadInfo QuadInfo;
			QuadInfo.Model = TextModel;
			QuadInfo.Color = mColor;
			QuadInfo.TextureSRV = mFontAtlasAsset->GetSRV();
			QuadInfo.SubUV = Glyph.SubUV;
			QuadInfo.BlendMode = ERenderBlendMode::Transparent;
			QuadInfo.EnableDepthTest = mEnableDepthTest;
			QuadInfo.EnableDepthWrite = mEnableDepthWrite;
			QuadInfo.bIsBillboard = mbBillboard;
			QuadInfo.bCustomPivot = true;
			QuadInfo.PivotLocation = PivotTransform.Location;
			QuadInfo.LocalTransform = TextLocalModel;

			RenderCollector.AddQuadInfo(QuadInfo);

			TextLocation.y += WorldAdvance;
		}
	}

	inline void SetBillboardCamera(FCamera& camera) { mBillboardCamera = &camera; }
	inline void SetBillboard(bool billboard) { mbBillboard = billboard; }

	inline void SetText(const std::wstring& text) { mText = text; }
	inline const std::wstring& GetText() const { return mText; }

	inline void SetFontAtlasAsset(UFontAtlas* fontAtlasAsset) { mFontAtlasAsset = fontAtlasAsset; }

	inline void SetColor(const FVector4& color) { mColor = color; }
	inline void SetDepthState(bool enableDepthTest, bool enableDepthWrite) { mEnableDepthTest = enableDepthTest; mEnableDepthWrite = enableDepthWrite; }

private:
	FCamera* mBillboardCamera = nullptr;
	bool mbBillboard = false;
	std::wstring mText;
	UFontAtlas* mFontAtlasAsset = nullptr;
	FVector4 mColor = FVector4(1, 1, 1, 1);
	bool mEnableDepthTest = true;
	bool mEnableDepthWrite = true;
};
