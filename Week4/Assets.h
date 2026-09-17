#pragma once

#include "Core.h"
#include "FAsset.h"
#include "FFontAtlas.h"
#include "TArray.h"
#include "Vector.h"
#include "Matrix.h"
#include "FAABB.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <filesystem>
#include <ft2build.h>
#include FT_FREETYPE_H

class FFileManager;
class FFontManager;
class URenderer;

class FFileAssetSource : public FAssetSource
{
public:
	FFileAssetSource(FFileManager& InFileManager, const std::filesystem::path& InFilePath) : FileManager(InFileManager), FilePath(InFilePath) {}

	FString ReadFileToString() const;

private:
	FFileManager& FileManager;
	std::filesystem::path FilePath;
};

class FStaticMeshAsset : public FAsset
{
public:
	FStaticMeshAsset() = default;
	FStaticMeshAsset(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, uint32 InVertexCount);
	FStaticMeshAsset(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, uint32 InVertexCount, const uint32* InIndices, uint32 InIndexCount);

	inline Microsoft::WRL::ComPtr<ID3D11Buffer> GetVertexBuffer() const { return VertexBuffer; }
	inline uint32 GetVertexCount() const { return VertexCount; }
	inline Microsoft::WRL::ComPtr<ID3D11Buffer> GetIndexBuffer() const { return IndexBuffer; }
	inline uint32 GetIndexCount() const { return IndexCount; }
	inline const FAABB& GetLocalBoundingBox() const { return BoundingBox; }

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
	uint32 VertexCount;

	Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
	uint32 IndexCount;

	FAABB BoundingBox;
};

class FTexture2DAsset : public FAsset
{
public:
	FTexture2DAsset() = default;
	FTexture2DAsset(const FName& InAssetName, Microsoft::WRL::ComPtr<ID3D11Texture2D> InTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> InSRV)
		: FTexture2DAsset(InAssetName, EAssetType::Texture2D, InTexture, InSRV)
	{
	}

	inline Microsoft::WRL::ComPtr<ID3D11Texture2D> GetTexture() const { return Texture; }
	inline Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSRV() const { return SRV; }

	inline uint32 GetWidth() const { return Width; }
	inline uint32 GetHeight() const { return Height; }

	inline DXGI_FORMAT GetFormat() const { return Format; }

protected:
	FTexture2DAsset(const FName& InAssetName, EAssetType InAssetType, Microsoft::WRL::ComPtr<ID3D11Texture2D> InTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> InSRV)
		: FAsset(InAssetName, InAssetType)
		, Texture(InTexture)
		, SRV(InSRV)
	{
		if (Texture)
		{
			D3D11_TEXTURE2D_DESC TextureDesc = {};
			Texture->GetDesc(&TextureDesc);

			Width = TextureDesc.Width;
			Height = TextureDesc.Height;
			Format = TextureDesc.Format;
		}
	}

protected:
	Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;

	uint32 Width = 0;
	uint32 Height = 0;
	DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
};

class FTexture2DAssetLoader : public FAssetLoader
{
public:
	FTexture2DAssetLoader(URenderer& InRenderer) : Renderer(InRenderer) {}
	~FTexture2DAssetLoader() = default;

	virtual TSharedPtr<FAsset> LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;
	virtual void UnloadAsset(TSharedPtr<FAsset> Asset) override;
	virtual EAssetType GetAssetType() const override { return EAssetType::Texture2D; }

private:
	URenderer& Renderer;
};

class FFontAsset : public FAsset
{
public:
	FFontAsset() = default;
	FFontAsset(const FName& InAssetName, FT_Face InFace, FString&& InFileContent)
		: FAsset(InAssetName, EAssetType::Font)
		, Face(InFace)
		, FileContent(std::move(InFileContent))
	{
	}

	~FFontAsset()
	{
		if (Face)
		{
			FT_Done_Face(Face);
		}
	}

	inline FT_Face GetFace() const { return Face; }

private:
	FString FileContent;
	FT_Face Face;
};

class FFontAssetLoader : public FAssetLoader
{
public:
	FFontAssetLoader(FFontManager& InFontManager) : FontManager(InFontManager) {}
	~FFontAssetLoader() = default;

	virtual TSharedPtr<FAsset> LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;
	virtual void UnloadAsset(TSharedPtr<FAsset> Asset) override;
	virtual EAssetType GetAssetType() const override { return EAssetType::Font; }

private:
	FFontManager& FontManager;
};

class FFontAtlasAsset : public FTexture2DAsset, private FFontAtlasHandler
{
public:
	FFontAtlasAsset(const FName& InAssetName, URenderer& InRenderer, TSharedPtr<FFontAsset>& InFontAsset, uint32 InWidth, uint32 InHeight, uint32 InPaddingW, uint32 InPaddingH);

	inline TSharedPtr<FFontAtlas> GetFontAtlas() const { return FontAtlas; }
	void UpdateRegion(uint32 Left, uint32 Top, uint32 Right, uint32 Bottom, const void* Data, uint32 RowPitch);

protected:
	URenderer& Renderer;
private:
	bool HandleAddGlyph(FFontAtlas& FontAtlas, const FFontGlyph& InGlyph, const FFontGlyphBitmap& InBitmap) override;

private:
	TSharedPtr<FFontAsset> FontAsset;
	TSharedPtr<FFontAtlas> FontAtlas;
};

//Texture2DAsset을 받아 UV를 계산 후 저장하는 에셋
class FSpriteAtlasAsset : public FTexture2DAsset
{
public:
	//Cols. Rows : 아틀라스 텍스쳐에 들어가있는 스프라이트 col x row
	FSpriteAtlasAsset(const FName& InAssetName, URenderer& InRenderer, const TSharedPtr<FTexture2DAsset>& InSource, uint32 InCols, uint32 InRows, uint32 InFrameCount = 0);

	//FrameSUbUV : (시작 UV.x, 시작 UV.y, width, height)
	FSpriteAtlasAsset(const FName& InAssetName, URenderer& InRenderer, const TSharedPtr<FTexture2DAsset>& InSource, const TArray<FVector4>& InFrameSubUVs);

	inline int32 GetFrameCount() const { return FrameSubUVs.Num(); }
	const FVector4& GetFrameSubUV(int32 FrameIndex) const;

protected:
	URenderer& Renderer;
private:
	TArray<FVector4> FrameSubUVs;
};
