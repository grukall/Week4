#include "Assets.h"
#include "FileManager.h"
#include "Stb/stb_image.h"
#include "FLogManager.h"
#include "Renderer.h"
#include "FFontManager.h"
#include "MathUtility.h"
#include "ObjectFactory.h"
#include "Material.h"
FString FFileAssetSource::ReadFileToString() const
{
	return FileManager.ReadFileToString(FilePath);
}

void UStaticMesh::Initialize(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, uint32 InVertexCount)
{
	UAsset::Initialize(InAssetName);

	VertexCount = InVertexCount;
	VertexBuffer = InRenderer.CreateVertexBuffer(InVertices, InVertexCount);

	Vertices.Reserve(InVertexCount);
	for (uint32 i = 0; i < InVertexCount; ++i)
	{
		const FVertexSimple& Vertex = InVertices[i];
		Vertices.Add(Vertex);
		BoundingBox.ExpandToInclude(FVector(Vertex.x, Vertex.y, Vertex.z));
	}

	// 인덱스가 없는 메시라 섹션을 만들지 않는다. 섹션은 인덱스 구간을 가리키는 개념이다.
}

void UStaticMesh::Initialize(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, int32 InVertexCount, const uint32* InIndices, int32 InIndexCount)
{
	UAsset::Initialize(InAssetName);

	VertexCount = InVertexCount;
	IndexCount = InIndexCount;

	VertexBuffer = InRenderer.CreateVertexBuffer(InVertices, InVertexCount);
	IndexBuffer = InRenderer.CreateIndexBuffer(InIndices, InIndexCount);

	Vertices.Reserve(InVertexCount);
	for (uint32 i = 0; i < InVertexCount; ++i)
	{
		Vertices.Add(InVertices[i]);
	}

	Indices.Reserve(InIndexCount);
	for (uint32 i = 0; i < InIndexCount; ++i)
	{
		Indices.Add(InIndices[i]);

		const FVertexSimple& Vertex = InVertices[InIndices[i]];
		BoundingBox.ExpandToInclude(FVector(Vertex.x, Vertex.y, Vertex.z));
	}

	// 머티리얼이 하나뿐인 메시라 인덱스 전체를 덮는 섹션 하나로 시작한다.
	// OBJ 로더가 usemtl 단위로 쪼갠 섹션을 넣어주면 이 자리가 여러 개가 된다.
	Sections.Add({ 0, static_cast<uint32>(InIndexCount), 0 });
}

void UStaticMesh::Initialize(const FName& InAssetName, URenderer& InRenderer, const FVertexSimple* InVertices, int32 InVertexCount, const uint32* InIndices, int32 InIndexCount, const FStaticMeshSection* InSections, int32 InSectionCount)
{
	UAsset::Initialize(InAssetName);

	VertexCount = InVertexCount;
	IndexCount = InIndexCount;

	VertexBuffer = InRenderer.CreateVertexBuffer(InVertices, InVertexCount);
	IndexBuffer = InRenderer.CreateIndexBuffer(InIndices, InIndexCount);

	Vertices.Reserve(InVertexCount);
	for (uint32 i = 0; i < InVertexCount; ++i)
	{
		Vertices.Add(InVertices[i]);
	}

	Indices.Reserve(InIndexCount);
	for (uint32 i = 0; i < InIndexCount; ++i)
	{
		Indices.Add(InIndices[i]);

		const FVertexSimple& Vertex = InVertices[InIndices[i]];
		BoundingBox.ExpandToInclude(FVector(Vertex.x, Vertex.y, Vertex.z));
	}

	for (uint32 i = 0; i < InSectionCount; ++i)
	{
		Sections.Add(InSections[i]);
	}
}

void UStaticMesh::BuildRenderBuffers(URenderer& InRenderer)
{
	VertexCount = Vertices.Num();
	IndexCount = Indices.Num();

	VertexBuffer = InRenderer.CreateVertexBuffer(Vertices.Data(), VertexCount);
	IndexBuffer = InRenderer.CreateIndexBuffer(Indices.Data(), IndexCount);

	for (uint32 i = 0; i < IndexCount; ++i)
	{
		const FVertexSimple& Vertex = Vertices[Indices[i]];
		BoundingBox.ExpandToInclude(FVector(Vertex.x, Vertex.y, Vertex.z));
	}
}

void UStaticMesh::SetData(const TArray<FVertexSimple>& InVertices, const TArray<uint32>& InIndices, const TArray<FStaticMeshSection>& InSections)
{
	Vertices = InVertices;
	Indices = InIndices;
	Sections = InSections;
}

void UStaticMesh::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Vertices;
	Ar << Indices;
	Ar << Sections;
	Ar << MaterialKeys;
}

void UStaticMesh::PostLoad(URenderer* Renderer)
{
	Super::PostLoad(Renderer);

	// TODO: Material re-linking logic

	if (Renderer)
		BuildRenderBuffers(*Renderer);
}

void UStaticMesh::SetMaterial(uint32 MaterialSlotIndex, UMaterial* InMaterial)
{
	if (MaterialSlotIndex >= Materials.Num()) {
		Materials.SetNum(MaterialSlotIndex + 1);
	}

	Materials[MaterialSlotIndex] = InMaterial;
}

UMaterial* UStaticMesh::GetMaterial(uint32 MaterialSlotIndex) const
{
	if (MaterialSlotIndex >= Materials.Num()) {
		return nullptr;
	}

	return Materials[MaterialSlotIndex];
}

void UStaticMesh::AddSection(const FStaticMeshSection& InSection)
{
	Sections.Add(InSection);
}

uint32 UStaticMesh::AddMaterial(UMaterial* InMaterial)
{
	if (InMaterial == nullptr)
	{
		return UINT32_MAX;
	}

	const int32 ExistingSlot =
		FindMaterialSlot(InMaterial);

	if (ExistingSlot >= 0)
	{
		return static_cast<uint32>(ExistingSlot);
	}

	Materials.Add(InMaterial);

	return Materials.Num() - 1;
}

int32 UStaticMesh::FindMaterialSlot(UMaterial* InMaterial) const
{
	if (InMaterial == nullptr)
	{
		return -1;
	}

	for (uint32 Index = 0; Index < Materials.Num(); ++Index)
	{
		if (Materials[Index] == InMaterial)
		{
			return static_cast<int32>(Index);
		}
	}

	return -1;
}

void UStaticMesh::SetSectionMaterial(uint32 SectionIndex, UMaterial* InMaterial)
{
	if (SectionIndex >= Sections.Num())
	{
		return;
	}

	if (InMaterial == nullptr)
	{
		return;
	}

	const uint32 MaterialSlotIndex = AddMaterial(InMaterial);

	if (MaterialSlotIndex == UINT32_MAX)
	{
		return;
	}

	Sections[SectionIndex].MaterialSlotIndex = MaterialSlotIndex;
}

UAsset* FTexture2DAssetLoader::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
	FFileAssetSource& FileSource = static_cast<FFileAssetSource&>(AssetSource);
	FString FileContent = FileSource.ReadFileToString();

	int32 Width, Height, Channels;
	stbi_uc* ImageData = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(FileContent.CStr()), FileContent.Len(), &Width, &Height, &Channels, 4);

	if (!ImageData)
	{
		UE_LOG_ERROR("Failed to load texture asset: %s", AssetName.ToString().CStr());
		return nullptr;
	}

	D3D11_TEXTURE2D_DESC TextureDesc = {};
	TextureDesc.Width = Width;
	TextureDesc.Height = Height;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Usage = D3D11_USAGE_IMMUTABLE;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;
	TextureDesc.MipLevels = 1;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture = Renderer.CreateTexture2D(TextureDesc, ImageData);

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = TextureDesc.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV = Renderer.CreateShaderResourceView(Texture, &SRVDesc);

	stbi_image_free(ImageData);

	return FObjectFactory::ConstructObject<UTexture2D>(AssetName, Texture, SRV);
}

void FTexture2DAssetLoader::UnloadAsset(UAsset* Asset)
{
	// NOTE: Nothing to do for now
}

UAsset* FFontAssetLoader::LoadAsset(const FName& AssetName, FAssetSource& AssetSource)
{
	FFileAssetSource& FileSource = static_cast<FFileAssetSource&>(AssetSource);
	FString FileContent = FileSource.ReadFileToString();

	FT_Library Library = FontManager.GetLibrary();

	FT_Face Face;
	FT_Error Err = FT_New_Memory_Face(Library, reinterpret_cast<const FT_Byte*>(FileContent.CStr()), FileContent.Len(), 0, &Face);
	if (Err)
	{
		UE_LOG_ERROR("Failed to load font asset: %s", AssetName.ToString().CStr());
		return nullptr;
	}

	const FT_UInt DefaultSize = 64; // 기본 폰트 크기 설정
	if (FT_Set_Pixel_Sizes(Face, 0, DefaultSize))
	{
		UE_LOG_ERROR("Failed to set font size for asset: %s", AssetName.ToString().CStr());
		FT_Done_Face(Face);
		return nullptr;
	}
	FString AssetPath = AssetName.ToString();

	return FObjectFactory::ConstructObject<UFont>(AssetName, Face, std::move(FileContent));
}

void FFontAssetLoader::UnloadAsset(UAsset* Asset)
{
	// Nothing to do for now
}

void UFontAtlas::UpdateRegion(uint32 Left, uint32 Top, uint32 Right, uint32 Bottom, const void* Data, uint32 RowPitch)
{
	if (!Renderer || !Texture || !Data)
	{
		return;
	}

	if (Right <= Left || Bottom <= Top)
	{
		return;
	}

	D3D11_BOX DestBox = {};
	DestBox.left = Left;
	DestBox.top = Top;
	DestBox.right = Right;
	DestBox.bottom = Bottom;
	DestBox.front = 0;
	DestBox.back = 1;

	Renderer->GetDeviceContext()->UpdateSubresource(Texture.Get(), 0, &DestBox, Data, RowPitch, 0);
}

UFontAtlas::~UFontAtlas()
{
	delete FontAtlas;
	FontAtlas = nullptr;
}

void UFontAtlas::Initialize(const FName& InAssetName, URenderer& InRenderer, UFont* InFontAsset, uint32 InWidth, uint32 InHeight, uint32 InPaddingW, uint32 InPaddingH)
{
	UTexture2D::Initialize(InAssetName, nullptr, nullptr);

	Renderer = &InRenderer;
	FontAsset = InFontAsset;

	if (!FontAsset)
	{
		UE_LOG_ERROR("Font atlas '%s' has no source font", InAssetName.ToString().CStr());
		return;
	}

	delete FontAtlas;
	FontAtlas = new FFontAtlas(FontAsset->GetFace(), InWidth, InHeight, InPaddingW, InPaddingH);

	D3D11_TEXTURE2D_DESC TextureDesc = {};
	TextureDesc.Width = InWidth;
	TextureDesc.Height = InHeight;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = DXGI_FORMAT_R8_UNORM;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;

	Texture = Renderer->CreateTexture2D(TextureDesc);

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = TextureDesc.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;

	SRV = Renderer->CreateShaderResourceView(Texture, &SRVDesc);

	Width = InWidth;
	Height = InHeight;
	Format = TextureDesc.Format;

	FontAtlas->SetAtlasHandler(*this);
}

bool UFontAtlas::HandleAddGlyph(FFontAtlas& FontAtlas, const FFontGlyph& InGlyph, const FFontGlyphBitmap& InBitmap)
{
	UpdateRegion(InBitmap.Left, InBitmap.Top, InBitmap.Right, InBitmap.Bottom, InBitmap.Buffer, static_cast<uint32>(InBitmap.Pitch));

	return true;
}

void USpriteAtlas::Initialize(const FName& InAssetName, URenderer& InRenderer, UTexture2D* InSource, uint32 InCols, uint32 InRows, uint32 InFrameCount)
{
	UTexture2D::Initialize(InAssetName,
		InSource ? InSource->GetTexture() : Microsoft::WRL::ComPtr<ID3D11Texture2D>(),
		InSource ? InSource->GetSRV() : Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>());

	Renderer = &InRenderer;

	if (!InSource)
	{
		UE_LOG_ERROR("Sprite atlas '%s' has no source texture", InAssetName.ToString().CStr());
		return;
	}

	if (InCols == 0 || InRows == 0)
	{
		UE_LOG_ERROR("Sprite atlas '%s' has zero columns or rows", InAssetName.ToString().CStr());
		return;
	}

	const uint32 CellCount = InCols * InRows;
	const uint32 FrameCount = (InFrameCount == 0) ? CellCount : FPlatformMath::Min(InFrameCount, CellCount);

	const float FrameW = 1.0f / static_cast<float>(InCols);
	const float FrameH = 1.0f / static_cast<float>(InRows);

	FrameSubUVs.Reserve(FrameCount);
	for (uint32 i = 0; i < FrameCount; ++i)
	{
		const uint32 Col = i % InCols;
		const uint32 Row = i / InCols;

		FrameSubUVs.Add(FVector4(Col * FrameW, Row * FrameH, FrameW, FrameH));
	}
}

void USpriteAtlas::Initialize(const FName& InAssetName, URenderer& InRenderer, UTexture2D* InSource, const TArray<FVector4>& InFrameSubUVs)
{
	UTexture2D::Initialize(InAssetName,
		InSource ? InSource->GetTexture() : Microsoft::WRL::ComPtr<ID3D11Texture2D>(),
		InSource ? InSource->GetSRV() : Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>());

	Renderer = &InRenderer;
	FrameSubUVs = InFrameSubUVs;
}

const FVector4& USpriteAtlas::GetFrameSubUV(int32 FrameIndex) const
{
	static const FVector4 WholeTexture(0.f, 0.f, 1.f, 1.f);

	if (FrameSubUVs.IsEmpty())
	{
		return WholeTexture;
	}

	if (FrameIndex < 0 || FrameIndex >= FrameSubUVs.Num())
	{
		return FrameSubUVs[0];
	}

	return FrameSubUVs[static_cast<uint32>(FrameIndex)];
}
