#pragma once

#include "Core.h"
#include "UAsset.h"
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
#include "StaticMesh.h"

class FFileManager;
class FFontManager;
class URenderer;
class UTexture2D;
class UMaterial;

class FFileAssetSource : public FAssetSource
{
public:
	FFileAssetSource(FFileManager& InFileManager, const std::filesystem::path& InFilePath) : FileManager(InFileManager), FilePath(InFilePath) {}

	FString ReadFileToString() const;

	FFileManager& GetFileManager() const { return FileManager;  }

	std::filesystem::path GetFilePath() const { return FilePath; }

private:
	FFileManager& FileManager;
	std::filesystem::path FilePath;
};

class UStaticMesh : public UAsset
{
	REFLECT_CLASS(UStaticMesh, UAsset)
public:
	UStaticMesh() = default;

	using UAsset::Initialize;
	void Initialize(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, uint32 InVertexCount);
	void Initialize(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, int32 InVertexCount, const uint32* InIndices, int32 InIndexCount);
	void Initialize(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, int32 InVertexCount, const uint32* InIndices, int32 InIndexCount, const FStaticMeshSection* InSections, int32 InSectionCount);

	inline Microsoft::WRL::ComPtr<ID3D11Buffer> GetVertexBuffer() const { return VertexBuffer; }
	inline uint32 GetVertexCount() const { return VertexCount; }
	inline Microsoft::WRL::ComPtr<ID3D11Buffer> GetIndexBuffer() const { return IndexBuffer; }
	inline uint32 GetIndexCount() const { return IndexCount; }
	inline const FAABB& GetLocalBoundingBox() const { return BoundingBox; }

	// CPU 원본. 레이캐스트처럼 삼각형을 직접 훑어야 하는 쪽에서 쓴다.
	inline const TArray<FVertexSimple>& GetVertices() const { return Vertices; }
	inline const TArray<uint32>& GetIndices() const { return Indices; }
	inline const TArray<FStaticMeshSection>& GetSections() const { return Sections; }
	TArray<FStaticMeshSection>& GetSection(){ return Sections; }
	// UMaterial이 들어오기 전까지 쓰는 임시 표면 정보.
	void SetMaterial(uint32 MaterialSlotIndex, UMaterial* InMaterial);

	UMaterial* GetMaterial(uint32 MaterialSlotIndex) const;

	inline uint32 GetMaterialCount() const {
		return Materials.Num();
	}

	inline const TArray<UMaterial*>& GetMaterials() const {
		return Materials;
	}

	void AddSection(const FStaticMeshSection& InSection);

	inline uint32 GetSectionCount() const {
		return Sections.Num();
	}

	uint32 AddMaterial(UMaterial* InMaterial);

	// 슬롯만 비운다. 머티리얼 자체의 수명은 이 메시를 만든 로더가 책임진다.
	inline void ClearMaterials() { Materials.Empty(); }

	// .uasset에는 UMaterial* 포인터를 그대로 저장할 수 없어서(FArchive는 raw memcpy라 의미 없는 값이 됨),
	// 슬롯 순서대로 FAssetManager 조회 키를 대신 저장한다. 로드 후 이 키들로 실제 UMaterial*를 다시 구한다.
	inline void SetMaterialKeys(const TArray<FName>& InKeys) { MaterialKeys = InKeys; }
	inline const TArray<FName>& GetMaterialKeys() const { return MaterialKeys; }

	int32 FindMaterialSlot(UMaterial* InMaterial) const;

	void SetSectionMaterial(uint32 SectionIndex, UMaterial* InMaterial);

	void BuildRenderBuffers(URenderer& InRenderer);
	void SetData(const TArray<FVertexSimple>& InVertices, const TArray<uint32>& InIndices, const TArray<FStaticMeshSection>& InSections);
	void Serialize(FArchive& Ar) override;

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
	uint32 VertexCount = 0;

	Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
	uint32 IndexCount = 0;
	FAABB BoundingBox;

	TArray<FVertexSimple> Vertices;
	TArray<uint32> Indices;

	TArray<FStaticMeshSection> Sections;
	TArray<UMaterial*> Materials;

	// Materials와 같은 순서. .uasset에 저장/복원되는 건 이 키 배열뿐이다.
	TArray<FName> MaterialKeys;
};

class UTexture2D : public UAsset
{
	REFLECT_CLASS(UTexture2D, UAsset)
public:
	UTexture2D() = default;

	using UAsset::Initialize;
	void Initialize(const FName& InAssetName, Microsoft::WRL::ComPtr<ID3D11Texture2D> InTexture, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> InSRV)
	{
		UAsset::Initialize(InAssetName);

		Texture = InTexture;
		SRV = InSRV;

		if (Texture)
		{
			D3D11_TEXTURE2D_DESC TextureDesc = {};
			Texture->GetDesc(&TextureDesc);

			Width = TextureDesc.Width;
			Height = TextureDesc.Height;
			Format = TextureDesc.Format;
		}
	}

	inline const Microsoft::WRL::ComPtr<ID3D11Texture2D>& GetTexture() const { return Texture; }
	inline const Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& GetSRV() const { return SRV; }

	inline uint32 GetWidth() const { return Width; }
	inline uint32 GetHeight() const { return Height; }

	inline DXGI_FORMAT GetFormat() const { return Format; }
	
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

	virtual UAsset* LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;
	virtual void UnloadAsset(UAsset* Asset) override;

private:
	URenderer& Renderer;
};

class UFont : public UAsset
{
	REFLECT_CLASS(UFont, UAsset)
public:
	UFont() = default;

	using UAsset::Initialize;
	void Initialize(const FName& InAssetName, FT_Face InFace, FString&& InFileContent)
	{
		UAsset::Initialize(InAssetName);

		Face = InFace;
		FileContent = std::move(InFileContent);
	}

	~UFont()
	{
		if (Face)
		{
			FT_Done_Face(Face);
		}
	}

	inline FT_Face GetFace() const { return Face; }

private:
	FString FileContent;
	FT_Face Face = nullptr;
};

class FFontAssetLoader : public FAssetLoader
{
public:
	FFontAssetLoader(FFontManager& InFontManager) : FontManager(InFontManager) {}
	~FFontAssetLoader() = default;

	virtual UAsset* LoadAsset(const FName& AssetName, FAssetSource& AssetSource) override;
	virtual void UnloadAsset(UAsset* Asset) override;

private:
	FFontManager& FontManager;
};

class UFontAtlas : public UTexture2D, private FFontAtlasHandler
{
	REFLECT_CLASS(UFontAtlas, UTexture2D)
public:
	UFontAtlas() = default;
	~UFontAtlas();

	void Initialize(const FName& InAssetName, URenderer& InRenderer, UFont* InFontAsset, uint32 InWidth, uint32 InHeight, uint32 InPaddingW, uint32 InPaddingH);

	inline FFontAtlas* GetFontAtlas() const { return FontAtlas; }
	void UpdateRegion(uint32 Left, uint32 Top, uint32 Right, uint32 Bottom, const void* Data, uint32 RowPitch);

protected:
	URenderer* Renderer = nullptr;
private:
	bool HandleAddGlyph(FFontAtlas& FontAtlas, const FFontGlyph& InGlyph, const FFontGlyphBitmap& InBitmap) override;

private:
	UFont* FontAsset = nullptr;
	FFontAtlas* FontAtlas = nullptr;
};

//Texture2DAsset을 받아 UV를 계산 후 저장하는 에셋
class USpriteAtlas : public UTexture2D
{
	REFLECT_CLASS(USpriteAtlas, UTexture2D)
public:
	USpriteAtlas() = default;


	//Cols. Rows : 아틀라스 텍스쳐에 들어가있는 스프라이트 col x row
	void Initialize(const FName& InAssetName, URenderer& InRenderer, UTexture2D* InSource, uint32 InCols, uint32 InRows, uint32 InFrameCount = 0);

	//FrameSUbUV : (시작 UV.x, 시작 UV.y, width, height)
	void Initialize(const FName& InAssetName, URenderer& InRenderer, UTexture2D* InSource, const TArray<FVector4>& InFrameSubUVs);

	inline int32 GetFrameCount() const { return FrameSubUVs.Num(); }
	const FVector4& GetFrameSubUV(int32 FrameIndex) const;

protected:
	URenderer* Renderer = nullptr;
private:
	TArray<FVector4> FrameSubUVs;
};
