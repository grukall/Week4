#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include "imgui.h"

class FAssetManager;
class FGraphicsManager;
class UStaticMesh;
class UMaterial;
class UAsset;
class UTexture2D;
struct FThumbnailRenderTarget
{
	ID3D11Texture2D* Texture = nullptr;
	ID3D11RenderTargetView* RTV = nullptr;
	ID3D11ShaderResourceView* SRV = nullptr;
	ID3D11Texture2D* DepthStencil = nullptr;
	ID3D11DepthStencilView* DSV = nullptr;
};

class FThumbnailManager
{
public:
	static FThumbnailManager& Get()
	{
		static FThumbnailManager Instance;
		return Instance;
	}

	void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, FAssetManager* assetManager, FGraphicsManager* graphicsManager);
	void Shutdown();

	ImTextureID GetThumbnail(const std::filesystem::path& Path, bool IsDirectory);

	ImTextureID GetUAssetThumbnail(const std::filesystem::path& Path);

	// 로드된 인스턴스로 썸네일을 얻는다. 파일 위치는 GUID로 레지스트리에 물어본다 —
	// UAsset::AssetName은 표시용 이름이라 경로로 쓸 수 없다.
	ImTextureID GetAssetThumbnail(UAsset* Asset);
	ImTextureID GetStaticMeshThumbnail(const std::filesystem::path& Path, UStaticMesh* Mesh);
	ImTextureID GetMaterialThumbnail(const std::filesystem::path& Path, UMaterial* Material);

	ImTextureID GetDirectoryThumbnail();
	ImTextureID GetFontThumbnail();
	ImTextureID GetShaderThumbnail();
	ImTextureID GetFileThumbnail();

private:
	FThumbnailManager() = default;
	~FThumbnailManager() = default;

	FThumbnailManager(const FThumbnailManager&) = delete;
	FThumbnailManager& operator=(const FThumbnailManager&) = delete;

	ID3D11ShaderResourceView* LoadStaticMeshThumbnail(const std::filesystem::path& Path, UStaticMesh* Mesh, UMaterial* OverrideMaterial = nullptr);
	ID3D11ShaderResourceView* LoadMaterialThumbnail(const std::filesystem::path& Path, UMaterial* Material);
	ID3D11ShaderResourceView* LoadTextureThumbnail(const std::filesystem::path& Path);
	ID3D11ShaderResourceView* CreateIconTexture(const std::vector<uint32_t>& Pixels, UINT Width, UINT Height);

	void CreateThumbnailRenderTarget(int width, int height);
	void ReleaseThumbnailRenderTarget();

	bool SaveThumbnailDDS(const std::filesystem::path& Path, ID3D11Texture2D* SourceTexture);
	ID3D11ShaderResourceView* LoadThumbnailDDS(const std::filesystem::path& Path);
	std::filesystem::path GetThumbnailCachePath(const std::filesystem::path& AssetPath) const;

	// 캐시 키. 등록된 에셋이면 GUID, 아니면 정규화한 경로.
	std::string MakeThumbnailKey(const std::filesystem::path& AssetPath) const;

private:
	ID3D11Device* mDevice = nullptr;
	ID3D11DeviceContext* mContext = nullptr;
	FAssetManager* mAssetManager = nullptr;
	FGraphicsManager* mGraphicsManager = nullptr;

	FThumbnailRenderTarget mThumbRT;

	UStaticMesh* mMaterialPreviewSphere = nullptr;

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> mThumbnailCache;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFileIconSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mDirectoryThumbnail;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFontIconSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mShaderIconSRV;
};