#pragma once

#include <d3d11.h>
#include <filesystem>
#include <unordered_map>
#include <string>
#include "ImGui/imgui.h"

class UStaticMesh;
class FAssetManager;
class FGraphicsManager;

class FThumbnailManager {
public:
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, FAssetManager* assetManager, FGraphicsManager* graphicsManager);
    void Shutdown();

    // 외부에서 에셋 매니저 등을 넘겨받아 썸네일을 갱신할 수 있게 준비
    ImTextureID GetThumbnail(const std::filesystem::path& path, bool isDirectory);

private:
    FAssetManager* mAssetManager = nullptr;
    FGraphicsManager* mGraphicsManager = nullptr;

    ID3D11Device* mDevice = nullptr;
    ID3D11DeviceContext* mContext = nullptr;

    std::unordered_map<std::string, ID3D11ShaderResourceView*> mThumbnailCache;

    ID3D11ShaderResourceView* mFolderIconSRV = nullptr;
    ID3D11ShaderResourceView* mFileIconSRV = nullptr;

    // 3D 에셋 썸네일 생성 함수
    ID3D11ShaderResourceView* LoadStaticMeshThumbnail(const std::filesystem::path& path);
    ID3D11ShaderResourceView* LoadTextureThumbnail(const std::filesystem::path& path);

    // 썸네일 전용 오프스크린 렌더 타겟 구조체
    struct FThumbnailRenderTarget {
        ID3D11Texture2D* Texture = nullptr;
        ID3D11RenderTargetView* RTV = nullptr;
        ID3D11ShaderResourceView* SRV = nullptr;
        ID3D11Texture2D* DepthStencil = nullptr;
        ID3D11DepthStencilView* DSV = nullptr;
    };
    FThumbnailRenderTarget mThumbRT;

    void CreateThumbnailRenderTarget(int width, int height);
    void ReleaseThumbnailRenderTarget();
};