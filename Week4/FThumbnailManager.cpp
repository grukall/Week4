#include "FThumbnailManager.h"
#include "FAssetManager.h"
#include "stb_image.h"
#include "FAABB.h"
#include "StaticMesh.h"
#include "Material.h"
#include "FEditorViewportClient.h"
#include "GraphicsManager.h"
#include "Renderer.h"
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <cstring>

#pragma pack(push, 1)

struct FDDS_PIXELFORMAT
{
	uint32_t Size;
	uint32_t Flags;
	uint32_t FourCC;
	uint32_t RGBBitCount;
	uint32_t RBitMask;
	uint32_t GBitMask;
	uint32_t ABitMast;
};

struct FDDS_HEADER
{
	uint32_t Size;
	uint32_t Flags;
	uint32_t Height;
	uint32_t Width;
	uint32_t PitchOrLinearSize;
	uint32_t Depth;
	uint32_t MipMapCount;
	uint32_t Reserved1[11];
	FDDS_PIXELFORMAT PixelFormat;
	uint32_t Caps;
	uint32_t Caps2;
	uint32_t Caps3;
	uint32_t Caps4;
	uint32_t Reserved2;
};

struct FDDS_HEADER_DXT10
{
	uint32_t DXGIFormat;
	uint32_t ResourceDimension;
	uint32_t MiscFlag;
	uint32_t ArraySize;
	uint32_t MiscFlags2;
};

#pragma pack(pop)

static constexpr uint32_t DDS_MAGIC = 0x20534444;
static constexpr uint32_t DDS_HEADER_FLAGS = 0x0002100F;
static constexpr uint32_t DDS_PIXELFORMAT_FLAGS_FOURCC = 0x00000004;
static constexpr uint32_t DDS_CAPS_TEXTURE = 0x00001000;
static constexpr uint32_t DDS_FOURCC_DX10 = 0x30315844;

static uint32_t MakeRGBA(uint8_t R, uint8_t G, uint8_t B, uint8_t A = 255)
{
	return static_cast<uint32_t>(R)
		| (static_cast<uint32_t>(G) << 8)
		| (static_cast<uint32_t>(B) << 16)
		| (static_cast<uint32_t>(A) << 24);
}

void FThumbnailManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, FAssetManager* assetManager, FGraphicsManager* graphicsManager)
{
	mDevice = device;
	mContext = context;
	mAssetManager = assetManager;
	mGraphicsManager = graphicsManager;

	CreateThumbnailRenderTarget(128, 128);

	mMaterialPreviewSphere = nullptr;

	if (mAssetManager)
	{
		mMaterialPreviewSphere = mAssetManager->GetAssetAs<UStaticMesh>(FName("SphereMesh"), true);

		if (!mMaterialPreviewSphere)
		{
			FAssetManager::Get().ForEachMetaInfo(
				[&](FAssetMetaInfo& MetaInfo)
				{
					if (mMaterialPreviewSphere)
					{
						return;
					}

					if (!MetaInfo.AssetClass)
					{
						return;
					}

					if (MetaInfo.Stem.ToString() != "SphereMesh")
					{
						return;
					}

					UAsset* Asset = FAssetManager::Get().GetAsset(MetaInfo.AssetName, true);

					if (Asset)
					{
						mMaterialPreviewSphere = Asset->Cast<UStaticMesh>();
					}
				});
		}
	}
}

void FThumbnailManager::Shutdown()
{
	ReleaseThumbnailRenderTarget();

	mThumbnailCache.clear();

	mFileIconSRV.Reset();
	mDirectoryThumbnail.Reset();
	mFontIconSRV.Reset();
	mShaderIconSRV.Reset();

	mMaterialPreviewSphere = nullptr;

	mAssetManager = nullptr;
	mGraphicsManager = nullptr;
	mDevice = nullptr;
	mContext = nullptr;
}

ImTextureID FThumbnailManager::GetThumbnail(const std::filesystem::path& Path, bool IsDirectory)
{
	if (IsDirectory)
	{
		return GetDirectoryThumbnail();
	}

	std::string Ext = Path.extension().string();

	std::transform(
		Ext.begin(),
		Ext.end(),
		Ext.begin(),
		[](unsigned char C)
		{
			return static_cast<char>(std::tolower(C));
		});

	if (Ext == ".png" ||
		Ext == ".jpg" ||
		Ext == ".jpeg" ||
		Ext == ".bmp")
	{
		const std::string Key = Path.lexically_normal().string();

		auto It = mThumbnailCache.find(Key);

		if (It != mThumbnailCache.end())
		{
			return reinterpret_cast<ImTextureID>(It->second.Get());
		}

		ID3D11ShaderResourceView* SRV = LoadTextureThumbnail(Path);

		if (!SRV)
		{
			return GetFileThumbnail();
		}

		mThumbnailCache[Key].Attach(SRV);

		return reinterpret_cast<ImTextureID>(mThumbnailCache[Key].Get());
	}

	if (Ext == ".uasset")
	{
		return GetUAssetThumbnail(Path);
	}

	if (Ext == ".ttf")
	{
		return GetFontThumbnail();
	}

	if (Ext == ".hlsl")
	{
		return GetShaderThumbnail();
	}

	return GetFileThumbnail();
}

ImTextureID FThumbnailManager::GetMaterialThumbnailInternal(const std::filesystem::path& Path, UMaterial* Material)
{
	if (!Material)
	{
		return GetFileThumbnail();
	}

	// 경로가 비어있지 않은 경우 파스 정규화 키 생성, 없으면 포인터 주소를 기반 키로 사용
	const std::string Key = !Path.empty()
		? Path.lexically_normal().string()
		: "MaterialPtr_" + std::to_string(reinterpret_cast<uintptr_t>(Material));

	auto It = mThumbnailCache.find(Key);

	if (It != mThumbnailCache.end())
	{
		return reinterpret_cast<ImTextureID>(It->second.Get());
	}

	// 경로 정보가 있으면 저장된 DDS 썸네일 캐시 파일 검색
	if (!Path.empty())
	{
		const std::filesystem::path DDSPath = GetThumbnailCachePath(Path);

		if (std::filesystem::exists(DDSPath))
		{
			ID3D11ShaderResourceView* SRV = LoadThumbnailDDS(DDSPath);

			if (SRV)
			{
				mThumbnailCache[Key].Attach(SRV);
				return reinterpret_cast<ImTextureID>(mThumbnailCache[Key].Get());
			}
		}
	}

	// DDS 캐시가 없는 경우 머티리얼 썸네일 새로 렌더링
	ID3D11ShaderResourceView* SRV = LoadMaterialThumbnail(Path, Material);

	if (!SRV)
	{
		return GetFileThumbnail();
	}

	mThumbnailCache[Key].Attach(SRV);

	return reinterpret_cast<ImTextureID>(mThumbnailCache[Key].Get());
}

ImTextureID FThumbnailManager::GetUAssetThumbnail(const std::filesystem::path& Path)
{
	if (!mAssetManager)
	{
		return GetFileThumbnail();
	}

	const std::string Stem = Path.stem().string();

	UAsset* Asset = mAssetManager->GetAsset(FName(Stem.c_str()), true);

	if (!Asset)
	{
		const std::string FullPath = Path.string();

		Asset = mAssetManager->GetAsset(FName(FullPath.c_str()), true);
	}

	if (!Asset)
	{
		FAssetManager::Get().ForEachMetaInfo(
			[&](FAssetMetaInfo& MetaInfo)
			{
				if (Asset)
				{
					return;
				}

				if (!MetaInfo.AssetClass)
				{
					return;
				}

				if (MetaInfo.Stem.ToString() != Stem)
				{
					return;
				}

				Asset = FAssetManager::Get().GetAsset(MetaInfo.AssetName, true);
			});
	}

	if (!Asset)
	{
		return GetFileThumbnail();
	}

	if (UStaticMesh* Mesh = Asset->Cast<UStaticMesh>())
	{
		const std::string Key = Path.lexically_normal().string();

		auto It = mThumbnailCache.find(Key);

		if (It != mThumbnailCache.end())
		{
			return reinterpret_cast<ImTextureID>(It->second.Get());
		}

		const std::filesystem::path DDSPath = GetThumbnailCachePath(Path);

		if (std::filesystem::exists(DDSPath))
		{
			ID3D11ShaderResourceView* SRV = LoadThumbnailDDS(DDSPath);

			if (SRV)
			{
				mThumbnailCache[Key].Attach(SRV);

				return reinterpret_cast<ImTextureID>(mThumbnailCache[Key].Get());
			}
		}

		ID3D11ShaderResourceView* SRV = LoadStaticMeshThumbnail(Path, Mesh);

		if (!SRV)
		{
			return GetFileThumbnail();
		}

		mThumbnailCache[Key].Attach(SRV);

		return reinterpret_cast<ImTextureID>(mThumbnailCache[Key].Get());
	}

	if (UMaterial* Material = Asset->Cast<UMaterial>())
	{
		return GetMaterialThumbnail(Path, Material);
	}

	return GetFileThumbnail();
}

ImTextureID FThumbnailManager::GetUAssetThumbnail(UAsset* Asset)
{
	if (!Asset)
	{
		return GetFileThumbnail();
	}

	if (UStaticMesh* Mesh = Asset->Cast<UStaticMesh>())
	{
		return GetStaticMeshThumbnail(Mesh);
	}

	if (UMaterial* Material = Asset->Cast<UMaterial>())
	{
		return GetMaterialThumbnail(Material);
	}

	return GetFileThumbnail();
}

ImTextureID FThumbnailManager::GetStaticMeshThumbnail(UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return GetFileThumbnail();
	}

	return GetStaticMeshThumbnail("", Mesh);
}

ImTextureID FThumbnailManager::GetStaticMeshThumbnail(const std::filesystem::path& Path, UStaticMesh* Mesh)
{
	if (!Mesh)
	{
		return GetFileThumbnail();
	}

	const std::string Key = !Path.empty()
		? Path.lexically_normal().string()
		: "StaticMeshPtr_" + std::to_string(reinterpret_cast<uintptr_t>(Mesh));

	auto It = mThumbnailCache.find(Key);
	if (It != mThumbnailCache.end())
	{
		return reinterpret_cast<ImTextureID>(It->second.Get());
	}

	if (!Path.empty())
	{
		const std::filesystem::path DDSPath = GetThumbnailCachePath(Path);
		if (std::filesystem::exists(DDSPath))
		{
			ID3D11ShaderResourceView* SRV = LoadThumbnailDDS(DDSPath);
			if (SRV)
			{
				mThumbnailCache[Key].Attach(SRV);
				return reinterpret_cast<ImTextureID>(mThumbnailCache[Key].Get());
			}
		}
	}

	ID3D11ShaderResourceView* SRV = LoadStaticMeshThumbnail(Path, Mesh);
	if (!SRV)
	{
		return GetFileThumbnail();
	}

	mThumbnailCache[Key].Attach(SRV);
	return reinterpret_cast<ImTextureID>(mThumbnailCache[Key].Get());
}

ImTextureID FThumbnailManager::GetMaterialThumbnail(UMaterial* Material)
{
	if (!Material)
	{
		return GetFileThumbnail();
	}

	return GetMaterialThumbnailInternal("", Material);
}

ImTextureID FThumbnailManager::GetMaterialThumbnail(const std::filesystem::path& Path, UMaterial* Material)
{
	return GetMaterialThumbnailInternal(Path, Material);
}

ID3D11ShaderResourceView* FThumbnailManager::LoadMaterialThumbnail(const std::filesystem::path& Path, UMaterial* Material)
{
	if (!Material)
	{
		return nullptr;
	}

	if (!mMaterialPreviewSphere)
	{
		return nullptr;
	}

	return LoadStaticMeshThumbnail(Path, mMaterialPreviewSphere, Material);
}

ID3D11ShaderResourceView* FThumbnailManager::LoadStaticMeshThumbnail(const std::filesystem::path& Path, UStaticMesh* Mesh, UMaterial* OverrideMaterial)
{
	constexpr UINT ThumbnailSize = 128;

	if (!mAssetManager ||
		!mGraphicsManager ||
		!mDevice ||
		!mContext ||
		!mThumbRT.Texture ||
		!mThumbRT.RTV ||
		!mThumbRT.DSV)
	{
		return nullptr;
	}

	if (!Mesh)
	{
		return nullptr;
	}

	const TArray<FStaticMeshSection>& Sections = Mesh->GetSections();

	if (Sections.IsEmpty())
	{
		return nullptr;
	}

	if (!Mesh->GetIndexBuffer() || Mesh->GetIndexCount() == 0)
	{
		return nullptr;
	}

	ID3D11RenderTargetView* PreviousRTV = nullptr;
	ID3D11DepthStencilView* PreviousDSV = nullptr;

	mContext->OMGetRenderTargets(
		1,
		&PreviousRTV,
		&PreviousDSV);

	constexpr UINT MaxViewports = 16;

	D3D11_VIEWPORT PreviousViewports[MaxViewports] = {};
	UINT PreviousViewportCount = MaxViewports;

	mContext->RSGetViewports(
		&PreviousViewportCount,
		PreviousViewports);

	FRenderCollector& RenderCollector = mGraphicsManager->GetRenderCollector();

	TArray<FRenderInfo> SavedRenderInfos = RenderCollector.RenderInfos;
	TArray<FRenderLineInfo> SavedLineInfos = RenderCollector.LineInfos;
	TArray<UPrimitiveComponent*> SavedPickTargets = RenderCollector.PickTargets;
	FCamera* SavedCamera = RenderCollector.Camera;

	auto RestoreState = [&]()
		{
			mContext->OMSetRenderTargets(
				1,
				&PreviousRTV,
				PreviousDSV);

			if (PreviousViewportCount > 0)
			{
				mContext->RSSetViewports(
					PreviousViewportCount,
					PreviousViewports);
			}

			RenderCollector.RenderInfos = SavedRenderInfos;
			RenderCollector.LineInfos = SavedLineInfos;
			RenderCollector.PickTargets = SavedPickTargets;
			RenderCollector.Camera = SavedCamera;

			if (PreviousRTV)
			{
				PreviousRTV->Release();
				PreviousRTV = nullptr;
			}

			if (PreviousDSV)
			{
				PreviousDSV->Release();
				PreviousDSV = nullptr;
			}
		};

	URenderer* Renderer = mGraphicsManager->GetRenderer();

	if (!Renderer)
	{
		RestoreState();
		return nullptr;
	}

	FEditorViewportClient ThumbnailViewport(*Renderer);

	ThumbnailViewport.mWidth = ThumbnailSize;
	ThumbnailViewport.mHeight = ThumbnailSize;
	ThumbnailViewport.mCamera.mFovDegree = 45.0f;

	const FAABB LocalBounds = Mesh->GetLocalBoundingBox();

	FVector LocalMin = LocalBounds.Min;
	FVector LocalMax = LocalBounds.Max;

	FVector Center = (LocalMin + LocalMax) * 0.5f;
	FVector Extent = (LocalMax - LocalMin) * 0.5f;

	float Radius = std::sqrt(
		Extent.x * Extent.x +
		Extent.y * Extent.y +
		Extent.z * Extent.z);

	if (Radius < 0.001f || std::isnan(Radius))
	{
		Radius = 1.0f;
		Center = FVector(0.0f, 0.0f, 0.0f);
	}

	const float CameraDistance = Radius * 3.0f;

	ThumbnailViewport.mCamera.Transform.Rotation = FRotator(-20.0f, 45.0f, 0.0f);

	FVector Forward = ThumbnailViewport.mCamera.GetForwardVector();

	ThumbnailViewport.mCamera.Transform.Location =
		FVector(1000.0f, 1000.0f, 1000.0f) -
		Forward * CameraDistance;

	FTransform MeshTransform;

	MeshTransform.Location =
		FVector(
			1000.0f - Center.x,
			1000.0f - Center.y,
			1000.0f - Center.z);

	MeshTransform.Rotation = FRotator(0.0f, 0.0f, 0.0f);
	MeshTransform.Scale = FVector(1.0f, 1.0f, 1.0f);

	const FMatrix WorldMatrix = MeshTransform.MakeMatrix();

	RenderCollector.RenderInfos.Empty();
	RenderCollector.LineInfos.Empty();
	RenderCollector.PickTargets.Empty();
	RenderCollector.Camera = &ThumbnailViewport.mCamera;

	for (uint32 SectionIndex = 0;
		SectionIndex < Sections.Num();
		++SectionIndex)
	{
		const FStaticMeshSection& Section = Sections[SectionIndex];

		UMaterial* Material = OverrideMaterial;

		if (!Material)
		{
			Material = Mesh->GetMaterial(
				Section.MaterialSlotIndex);
		}

		if (!Material)
		{
			Material = UMaterial::DefaultMaterial;
		}

		FRenderInfo RenderInfo{};

		RenderInfo.StaticMesh = Mesh;
		RenderInfo.Material = Material;
		RenderInfo.WorldTransformMatrix = WorldMatrix;
		RenderInfo.ObejctID = FObjectID{ 0, 0 };
		RenderInfo.SectionIndex = SectionIndex;

		RenderCollector.RenderInfos.Add(RenderInfo);
	}

	mContext->OMSetRenderTargets(1, &mThumbRT.RTV, mThumbRT.DSV);

	D3D11_VIEWPORT ThumbnailViewportDesc = {};

	ThumbnailViewportDesc.TopLeftX = 0.0f;
	ThumbnailViewportDesc.TopLeftY = 0.0f;
	ThumbnailViewportDesc.Width = static_cast<float>(ThumbnailSize);
	ThumbnailViewportDesc.Height = static_cast<float>(ThumbnailSize);
	ThumbnailViewportDesc.MinDepth = 0.0f;
	ThumbnailViewportDesc.MaxDepth = 1.0f;

	mContext->RSSetViewports(
		1,
		&ThumbnailViewportDesc);

	const float ClearColor[4] =
	{
	0.15f,
	0.15f,
	0.15f,
	1.0f
	};

	mContext->ClearRenderTargetView(
		mThumbRT.RTV,
		ClearColor);

	mContext->ClearDepthStencilView(
		mThumbRT.DSV,
		D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
		1.0f,
		0);

	mGraphicsManager->Prepare(
		&ThumbnailViewport.mCamera,
		static_cast<float>(ThumbnailSize),
		static_cast<float>(ThumbnailSize));

	RenderCollector.LineInfos.Empty();

	mContext->IASetPrimitiveTopology(
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	bool bSavedGridFlag =
		FShowFlags::Get().IsEnabled(EShowFlag::Grid);

	FShowFlags::Get().SetEnabled(
		EShowFlag::Grid,
		false);

	mGraphicsManager->Render();

	FShowFlags::Get().SetEnabled(
		EShowFlag::Grid,
		bSavedGridFlag);

	D3D11_TEXTURE2D_DESC TextureDesc = {};

	mThumbRT.Texture->GetDesc(
		&TextureDesc);

	Microsoft::WRL::ComPtr<ID3D11Texture2D> ThumbnailTexture;

	HRESULT HR =
		mDevice->CreateTexture2D(
			&TextureDesc,
			nullptr,
			ThumbnailTexture.GetAddressOf());

	if (FAILED(HR))
	{
		RestoreState();
		return nullptr;
	}

	mContext->CopyResource(
		ThumbnailTexture.Get(),
		mThumbRT.Texture);

	if (!Path.empty())
	{
		const std::filesystem::path DDSPath = GetThumbnailCachePath(Path);
		SaveThumbnailDDS(DDSPath, mThumbRT.Texture);
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};

	SRVDesc.Format = TextureDesc.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = TextureDesc.MipLevels;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ThumbnailSRV;

	HR =
		mDevice->CreateShaderResourceView(
			ThumbnailTexture.Get(),
			&SRVDesc,
			ThumbnailSRV.GetAddressOf());

	if (FAILED(HR))
	{
		RestoreState();
		return nullptr;
	}

	RestoreState();

	return ThumbnailSRV.Detach();
}

ID3D11ShaderResourceView* FThumbnailManager::LoadTextureThumbnail(const std::filesystem::path& Path)
{
	if (!mDevice)
	{
		return nullptr;
	}

	int Width = 0;
	int Height = 0;
	int Channels = 0;

	unsigned char* Data =
		stbi_load(
			Path.string().c_str(),
			&Width,
			&Height,
			&Channels,
			4);

	if (!Data)
	{
		return nullptr;
	}

	D3D11_TEXTURE2D_DESC Desc = {};

	Desc.Width = Width;
	Desc.Height = Height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA InitData = {};

	InitData.pSysMem = Data;
	InitData.SysMemPitch = Width * 4;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;

	HRESULT HR =
		mDevice->CreateTexture2D(
			&Desc,
			&InitData,
			Texture.GetAddressOf());

	stbi_image_free(Data);

	if (FAILED(HR))
	{
		return nullptr;
	}

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;

	HR =
		mDevice->CreateShaderResourceView(
			Texture.Get(),
			nullptr,
			SRV.GetAddressOf());

	if (FAILED(HR))
	{
		return nullptr;
	}

	return SRV.Detach();
}

ID3D11ShaderResourceView* FThumbnailManager::CreateIconTexture(const std::vector<uint32_t>& Pixels, UINT Width, UINT Height)
{
	if (!mDevice)
	{
		return nullptr;
	}

	if (Pixels.size() != static_cast<size_t>(Width) * Height)
	{
		return nullptr;
	}

	D3D11_TEXTURE2D_DESC TextureDesc = {};

	TextureDesc.Width = Width;
	TextureDesc.Height = Height;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA InitialData = {};

	InitialData.pSysMem = Pixels.data();
	InitialData.SysMemPitch = Width * sizeof(uint32_t);

	Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;

	HRESULT HR =
		mDevice->CreateTexture2D(
			&TextureDesc,
			&InitialData,
			Texture.GetAddressOf());

	if (FAILED(HR))
	{
		return nullptr;
	}

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;

	HR =
		mDevice->CreateShaderResourceView(
			Texture.Get(),
			nullptr,
			SRV.GetAddressOf());

	if (FAILED(HR))
	{
		return nullptr;
	}

	return SRV.Detach();
}

ImTextureID FThumbnailManager::GetDirectoryThumbnail()
{
	if (mDirectoryThumbnail)
	{
		return reinterpret_cast<ImTextureID>(mDirectoryThumbnail.Get());
	}

	constexpr UINT Width = 128;
	constexpr UINT Height = 128;

	std::vector<uint32_t> Pixels(
		Width * Height,
		MakeRGBA(64, 64, 64));

	auto SetPixel =
		[&](UINT X, UINT Y, uint32_t Color)
		{
			if (X < Width && Y < Height)
			{
				Pixels[Y * Width + X] = Color;
			}
		};

	for (UINT Y = 32; Y < 96; ++Y)
	{
		for (UINT X = 20; X < 108; ++X)
		{
			SetPixel(
				X,
				Y,
				MakeRGBA(25, 25, 25));
		}
	}

	for (UINT Y = 40; Y < 92; ++Y)
	{
		for (UINT X = 24; X < 104; ++X)
		{
			SetPixel(
				X,
				Y,
				MakeRGBA(61, 139, 255));
		}
	}

	for (UINT Y = 32; Y < 44; ++Y)
	{
		for (UINT X = 28; X < 68; ++X)
		{
			SetPixel(
				X,
				Y,
				MakeRGBA(105, 167, 255));
		}
	}

	ID3D11ShaderResourceView* SRV =
		CreateIconTexture(
			Pixels,
			Width,
			Height);

	if (!SRV)
	{
		return ImTextureID{};
	}

	mDirectoryThumbnail.Attach(SRV);

	return reinterpret_cast<ImTextureID>(
		mDirectoryThumbnail.Get());
}

ImTextureID FThumbnailManager::GetFontThumbnail()
{
	if (mFontIconSRV)
	{
		return reinterpret_cast<ImTextureID>(
			mFontIconSRV.Get());
	}

	constexpr UINT Width = 128;
	constexpr UINT Height = 128;

	std::vector<uint32_t> Pixels(
		Width * Height,
		MakeRGBA(55, 55, 55));

	auto SetPixel =
		[&](UINT X, UINT Y, uint32_t Color)
		{
			if (X < Width && Y < Height)
			{
				Pixels[Y * Width + X] = Color;
			}
		};

	for (UINT Y = 20; Y < 108; ++Y)
	{
		for (UINT X = 28; X < 100; ++X)
		{
			SetPixel(
				X,
				Y,
				MakeRGBA(235, 235, 235));
		}
	}

	for (UINT Y = 20; Y < 38; ++Y)
	{
		for (UINT X = 82; X < 100; ++X)
		{
			SetPixel(
				X,
				Y,
				MakeRGBA(180, 180, 180));
		}
	}

	for (UINT Y = 44; Y < 54; ++Y)
	{
		for (UINT X = 42; X < 86; ++X)
		{
			SetPixel(
				X,
				Y,
				MakeRGBA(65, 95, 170));
		}
	}

	for (UINT Y = 50; Y < 90; ++Y)
	{
		for (UINT X = 58; X < 70; ++X)
		{
			SetPixel(
				X,
				Y,
				MakeRGBA(65, 95, 170));
		}
	}

	ID3D11ShaderResourceView* SRV =
		CreateIconTexture(
			Pixels,
			Width,
			Height);

	if (!SRV)
	{
		return ImTextureID{};
	}

	mFontIconSRV.Attach(SRV);

	return reinterpret_cast<ImTextureID>(
		mFontIconSRV.Get());
}

ImTextureID FThumbnailManager::GetShaderThumbnail()
{
	if (mShaderIconSRV)
	{
		return reinterpret_cast<ImTextureID>(
			mShaderIconSRV.Get());
	}

	constexpr UINT Width = 128;
	constexpr UINT Height = 128;

	std::vector<uint32_t> Pixels(
		Width * Height,
		MakeRGBA(35, 35, 35));

	auto SetPixel =
		[&](UINT X, UINT Y, uint32_t Color)
		{
			if (X < Width && Y < Height)
			{
				Pixels[Y * Width + X] = Color;
			}
		};

	const uint32_t Green =
		MakeRGBA(80, 190, 110);

	const uint32_t White =
		MakeRGBA(220, 220, 220);

	for (UINT Y = 24; Y < 104; ++Y)
	{
		for (UINT X = 20; X < 108; ++X)
		{
			if (X < 26 || X >= 102 || Y < 30 || Y >= 98)
			{
				SetPixel(
					X,
					Y,
					MakeRGBA(60, 60, 60));
			}
		}
	}

	for (UINT Y = 35; Y < 41; ++Y)
	{
		for (UINT X = 32; X < 88; ++X)
		{
			SetPixel(
				X,
				Y,
				Green);
		}
	}

	for (UINT Y = 48; Y < 54; ++Y)
	{
		for (UINT X = 32; X < 78; ++X)
		{
			SetPixel(
				X,
				Y,
				White);
		}
	}

	for (UINT Y = 61; Y < 67; ++Y)
	{
		for (UINT X = 32; X < 92; ++X)
		{
			SetPixel(
				X,
				Y,
				Green);
		}
	}

	for (UINT Y = 74; Y < 80; ++Y)
	{
		for (UINT X = 32; X < 68; ++X)
		{
			SetPixel(
				X,
				Y,
				White);
		}
	}

	for (UINT I = 0; I < 20; ++I)
	{
		SetPixel(
			88 - I,
			45 + I,
			Green);

		SetPixel(
			88 - I,
			83 - I,
			Green);
	}

	ID3D11ShaderResourceView* SRV =
		CreateIconTexture(
			Pixels,
			Width,
			Height);

	if (!SRV)
	{
		return ImTextureID{};
	}

	mShaderIconSRV.Attach(SRV);

	return reinterpret_cast<ImTextureID>(
		mShaderIconSRV.Get());
}

ImTextureID FThumbnailManager::GetFileThumbnail()
{
	if (mFileIconSRV)
	{
		return reinterpret_cast<ImTextureID>(
			mFileIconSRV.Get());
	}

	constexpr UINT Width = 128;
	constexpr UINT Height = 128;

	std::vector<uint32_t> Pixels(
		Width * Height,
		MakeRGBA(55, 55, 55));

	auto SetPixel =
		[&](UINT X, UINT Y, uint32_t Color)
		{
			if (X < Width && Y < Height)
			{
				Pixels[Y * Width + X] = Color;
			}
		};

	for (UINT Y = 20; Y < 108; ++Y)
	{
		for (UINT X = 28; X < 100; ++X)
		{
			SetPixel(
				X,
				Y,
				MakeRGBA(225, 225, 225));
		}
	}

	for (UINT Y = 20; Y < 40; ++Y)
	{
		for (UINT X = 80; X < 100; ++X)
		{
			SetPixel(
				X,
				Y,
				MakeRGBA(180, 180, 180));
		}
	}

	for (UINT Y = 48; Y < 54; ++Y)
	{
		for (UINT X = 42; X < 86; ++X)
		{
			SetPixel(
				X,
				Y,
				MakeRGBA(110, 110, 110));
		}
	}

	for (UINT Y = 62; Y < 68; ++Y)
	{
		for (UINT X = 42; X < 82; ++X)
		{
			SetPixel(
				X,
				Y,
				MakeRGBA(110, 110, 110));
		}
	}

	for (UINT Y = 76; Y < 82; ++Y)
	{
		for (UINT X = 42; X < 76; ++X)
		{
			SetPixel(
				X,
				Y,
				MakeRGBA(110, 110, 110));
		}
	}

	ID3D11ShaderResourceView* SRV =
		CreateIconTexture(
			Pixels,
			Width,
			Height);

	if (!SRV)
	{
		return ImTextureID{};
	}

	mFileIconSRV.Attach(SRV);

	return reinterpret_cast<ImTextureID>(
		mFileIconSRV.Get());
}

void FThumbnailManager::CreateThumbnailRenderTarget(int width, int height)
{
	if (!mDevice)
	{
		return;
	}

	D3D11_TEXTURE2D_DESC Desc = {};

	Desc.Width = width;
	Desc.Height = height;
	Desc.MipLevels = 1;
	Desc.ArraySize = 1;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.BindFlags =
		D3D11_BIND_RENDER_TARGET |
		D3D11_BIND_SHADER_RESOURCE;

	mDevice->CreateTexture2D(
		&Desc,
		nullptr,
		&mThumbRT.Texture);

	mDevice->CreateRenderTargetView(
		mThumbRT.Texture,
		nullptr,
		&mThumbRT.RTV);

	mDevice->CreateShaderResourceView(
		mThumbRT.Texture,
		nullptr,
		&mThumbRT.SRV);

	D3D11_TEXTURE2D_DESC DepthDesc = Desc;

	DepthDesc.Format =
		DXGI_FORMAT_D24_UNORM_S8_UINT;

	DepthDesc.BindFlags =
		D3D11_BIND_DEPTH_STENCIL;

	mDevice->CreateTexture2D(
		&DepthDesc,
		nullptr,
		&mThumbRT.DepthStencil);

	mDevice->CreateDepthStencilView(
		mThumbRT.DepthStencil,
		nullptr,
		&mThumbRT.DSV);
}

void FThumbnailManager::ReleaseThumbnailRenderTarget()
{
	if (mThumbRT.DSV)
	{
		mThumbRT.DSV->Release();
		mThumbRT.DSV = nullptr;
	}

	if (mThumbRT.DepthStencil)
	{
		mThumbRT.DepthStencil->Release();
		mThumbRT.DepthStencil = nullptr;
	}

	if (mThumbRT.SRV)
	{
		mThumbRT.SRV->Release();
		mThumbRT.SRV = nullptr;
	}

	if (mThumbRT.RTV)
	{
		mThumbRT.RTV->Release();
		mThumbRT.RTV = nullptr;
	}

	if (mThumbRT.Texture)
	{
		mThumbRT.Texture->Release();
		mThumbRT.Texture = nullptr;
	}
}

bool FThumbnailManager::SaveThumbnailDDS(const std::filesystem::path& Path, ID3D11Texture2D* SourceTexture)
{
	if (!SourceTexture ||
		!mDevice ||
		!mContext)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC SourceDesc = {};

	SourceTexture->GetDesc(
		&SourceDesc);

	if (SourceDesc.SampleDesc.Count != 1)
	{
		return false;
	}

	if (SourceDesc.Format != DXGI_FORMAT_R8G8B8A8_UNORM &&
		SourceDesc.Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
		SourceDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
		SourceDesc.Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
	{
		return false;
	}

	D3D11_TEXTURE2D_DESC StagingDesc = {};

	StagingDesc.Width = SourceDesc.Width;
	StagingDesc.Height = SourceDesc.Height;
	StagingDesc.MipLevels = 1;
	StagingDesc.ArraySize = 1;
	StagingDesc.Format = SourceDesc.Format;
	StagingDesc.SampleDesc.Count = 1;
	StagingDesc.SampleDesc.Quality = 0;
	StagingDesc.Usage = D3D11_USAGE_STAGING;
	StagingDesc.BindFlags = 0;
	StagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	StagingDesc.MiscFlags = 0;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> StagingTexture;

	HRESULT HR =
		mDevice->CreateTexture2D(
			&StagingDesc,
			nullptr,
			StagingTexture.GetAddressOf());

	if (FAILED(HR))
	{
		return false;
	}

	mContext->CopyResource(
		StagingTexture.Get(),
		SourceTexture);

	D3D11_MAPPED_SUBRESOURCE Mapped = {};

	HR =
		mContext->Map(
			StagingTexture.Get(),
			0,
			D3D11_MAP_READ,
			0,
			&Mapped);

	if (FAILED(HR))
	{
		return false;
	}

	std::filesystem::create_directories(
		Path.parent_path());

	std::ofstream File(
		Path,
		std::ios::binary);

	if (!File.is_open())
	{
		mContext->Unmap(
			StagingTexture.Get(),
			0);

		return false;
	}

	FDDS_HEADER Header = {};

	Header.Size = 124;
	Header.Flags = DDS_HEADER_FLAGS;
	Header.Height = SourceDesc.Height;
	Header.Width = SourceDesc.Width;
	Header.PitchOrLinearSize =
		SourceDesc.Width * 4;
	Header.Depth = 0;
	Header.MipMapCount = 1;
	Header.PixelFormat.Size = 32;
	Header.PixelFormat.Flags =
		DDS_PIXELFORMAT_FLAGS_FOURCC;
	Header.PixelFormat.FourCC =
		DDS_FOURCC_DX10;
	Header.Caps =
		DDS_CAPS_TEXTURE;

	FDDS_HEADER_DXT10 DX10Header = {};

	DX10Header.DXGIFormat =
		static_cast<uint32_t>(
			SourceDesc.Format);

	DX10Header.ResourceDimension = 3;
	DX10Header.MiscFlag = 0;
	DX10Header.ArraySize = 1;
	DX10Header.MiscFlags2 = 0;

	File.write(
		reinterpret_cast<const char*>(&DDS_MAGIC),
		sizeof(DDS_MAGIC));

	File.write(
		reinterpret_cast<const char*>(&Header),
		sizeof(Header));

	File.write(
		reinterpret_cast<const char*>(&DX10Header),
		sizeof(DX10Header));

	const uint32_t RowBytes =
		SourceDesc.Width * 4;

	for (uint32_t Y = 0;
		Y < SourceDesc.Height;
		++Y)
	{
		const uint8_t* Row =
			static_cast<const uint8_t*>(Mapped.pData) +
			Mapped.RowPitch * Y;

		File.write(
			reinterpret_cast<const char*>(Row),
			RowBytes);
	}

	File.close();

	mContext->Unmap(
		StagingTexture.Get(),
		0);

	return true;
}

ID3D11ShaderResourceView* FThumbnailManager::LoadThumbnailDDS(const std::filesystem::path& Path)
{
	if (!mDevice)
	{
		return nullptr;
	}

	std::ifstream File(
		Path,
		std::ios::binary | std::ios::ate);

	if (!File.is_open())
	{
		return nullptr;
	}

	const std::streamsize FileSize =
		File.tellg();

	if (FileSize <= 0)
	{
		return nullptr;
	}

	File.seekg(
		0,
		std::ios::beg);

	std::vector<uint8_t> Data(
		static_cast<size_t>(FileSize));

	if (!File.read(
		reinterpret_cast<char*>(Data.data()),
		FileSize))
	{
		return nullptr;
	}

	size_t Offset = 0;

	if (Data.size() <
		sizeof(uint32_t) +
		sizeof(FDDS_HEADER) +
		sizeof(FDDS_HEADER_DXT10))
	{
		return nullptr;
	}

	uint32_t Magic = 0;

	memcpy(
		&Magic,
		Data.data(),
		sizeof(uint32_t));

	Offset += sizeof(uint32_t);

	if (Magic != DDS_MAGIC)
	{
		return nullptr;
	}

	FDDS_HEADER Header = {};

	memcpy(
		&Header,
		Data.data() + Offset,
		sizeof(FDDS_HEADER));

	Offset += sizeof(FDDS_HEADER);

	if (Header.Size != 124)
	{
		return nullptr;
	}

	if (Header.PixelFormat.FourCC != DDS_FOURCC_DX10)
	{
		return nullptr;
	}

	FDDS_HEADER_DXT10 DX10Header = {};

	memcpy(
		&DX10Header,
		Data.data() + Offset,
		sizeof(FDDS_HEADER_DXT10));

	Offset += sizeof(FDDS_HEADER_DXT10);

	if (DX10Header.ResourceDimension != 3 ||
		DX10Header.ArraySize != 1)
	{
		return nullptr;
	}

	const DXGI_FORMAT Format =
		static_cast<DXGI_FORMAT>(
			DX10Header.DXGIFormat);

	if (Format != DXGI_FORMAT_R8G8B8A8_UNORM &&
		Format != DXGI_FORMAT_R8G8B8A8_UNORM_SRGB &&
		Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
		Format != DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
	{
		return nullptr;
	}

	const uint32_t Width = Header.Width;
	const uint32_t Height = Header.Height;

	if (Width == 0 ||
		Height == 0)
	{
		return nullptr;
	}

	const size_t RowBytes =
		static_cast<size_t>(Width) * 4;

	const size_t RequiredBytes =
		RowBytes * Height;

	if (Offset + RequiredBytes > Data.size())
	{
		return nullptr;
	}

	std::vector<uint8_t> PixelData(
		RequiredBytes);

	memcpy(
		PixelData.data(),
		Data.data() + Offset,
		RequiredBytes);

	D3D11_TEXTURE2D_DESC TextureDesc = {};

	TextureDesc.Width = Width;
	TextureDesc.Height = Height;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = Format;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.SampleDesc.Quality = 0;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA InitialData = {};

	InitialData.pSysMem =
		PixelData.data();

	InitialData.SysMemPitch =
		static_cast<UINT>(RowBytes);

	Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;

	HRESULT HR =
		mDevice->CreateTexture2D(
			&TextureDesc,
			&InitialData,
			Texture.GetAddressOf());

	if (FAILED(HR))
	{
		return nullptr;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};

	SRVDesc.Format = Format;
	SRVDesc.ViewDimension =
		D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;

	HR =
		mDevice->CreateShaderResourceView(
			Texture.Get(),
			&SRVDesc,
			SRV.GetAddressOf());

	if (FAILED(HR))
	{
		return nullptr;
	}

	return SRV.Detach();
}

std::filesystem::path FThumbnailManager::GetThumbnailCachePath(const std::filesystem::path& AssetPath) const
{
	const std::filesystem::path CacheDirectory =
		"Saved/Thumbnails";

	const std::string NormalizedPath =
		AssetPath.lexically_normal().generic_string();

	const size_t HashValue =
		std::hash<std::string>{}(NormalizedPath);

	const std::string FileName =
		AssetPath.stem().string() +
		"_" +
		std::to_string(HashValue) +
		".dds";

	return CacheDirectory / FileName;
}