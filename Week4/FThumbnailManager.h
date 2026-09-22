#pragma once

#include <d3d11.h>
#include <wrl/client.h>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

// ImGui 텍스처 ID용 (프로젝트의 ImGui 헤더 경로에 맞게 수정하세요)
#include "imgui.h" 

class FAssetManager;
class FGraphicsManager;
class UStaticMesh;
class UMaterial;
class UAsset;

// 썸네일 렌더링을 위한 렌더타겟 구조체
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

	// 외부 노출 인터페이스
	ImTextureID GetThumbnail(const std::filesystem::path& Path, bool IsDirectory);
	ImTextureID GetUAssetThumbnail(const std::filesystem::path& Path);
	ImTextureID GetUAssetThumbnail(UAsset* Asset);
	ImTextureID GetStaticMeshThumbnail(UStaticMesh* Mesh);
	ImTextureID GetStaticMeshThumbnail(const std::filesystem::path& Path, UStaticMesh* Mesh);
	ImTextureID GetMaterialThumbnail(UMaterial* Material);
	ImTextureID GetMaterialThumbnail(const std::filesystem::path& Path, UMaterial* Material);

	// 기본 아이콘 텍스처 반환
	ImTextureID GetDirectoryThumbnail();
	ImTextureID GetFontThumbnail();
	ImTextureID GetShaderThumbnail();
	ImTextureID GetFileThumbnail();
private:
	FThumbnailManager() = default;
	~FThumbnailManager() = default;

	FThumbnailManager(const FThumbnailManager&) = delete;
	FThumbnailManager& operator=(const FThumbnailManager&) = delete;

	// 머티리얼 썸네일 내부 처리 (캐시 및 파일 저장 연동)
	ImTextureID GetMaterialThumbnailInternal(const std::filesystem::path& Path, UMaterial* Material);


	// 렌더링 및 텍스처 로드 함수들
	ID3D11ShaderResourceView* LoadMaterialThumbnail(const std::filesystem::path& Path, UMaterial* Material);
	ID3D11ShaderResourceView* LoadStaticMeshThumbnail(const std::filesystem::path& Path, UStaticMesh* Mesh, UMaterial* OverrideMaterial = nullptr);
	ID3D11ShaderResourceView* LoadTextureThumbnail(const std::filesystem::path& Path);
	ID3D11ShaderResourceView* CreateIconTexture(const std::vector<uint32_t>& Pixels, UINT Width, UINT Height);

	// 렌더타겟 관리
	void CreateThumbnailRenderTarget(int width, int height);
	void ReleaseThumbnailRenderTarget();

	// DDS 캐시 파일 입출력
	bool SaveThumbnailDDS(const std::filesystem::path& Path, ID3D11Texture2D* SourceTexture);
	ID3D11ShaderResourceView* LoadThumbnailDDS(const std::filesystem::path& Path);
	std::filesystem::path GetThumbnailCachePath(const std::filesystem::path& AssetPath) const;

private:
	// 매니저 및 디바이스 참조
	ID3D11Device* mDevice = nullptr;
	ID3D11DeviceContext* mContext = nullptr;
	FAssetManager* mAssetManager = nullptr;
	FGraphicsManager* mGraphicsManager = nullptr;

	// 썸네일 베이킹용 렌더 타겟
	FThumbnailRenderTarget mThumbRT;

	// 머티리얼 썸네일을 구울 때 사용할 기본 구체 메쉬
	UStaticMesh* mMaterialPreviewSphere = nullptr;

	// 메모리 캐시 (Key: 경로 정규화 문자열 또는 포인터 주소)
	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> mThumbnailCache;

	// 기본 아이콘 SRV 캐시
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFileIconSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mDirectoryThumbnail;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mFontIconSRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mShaderIconSRV;
};