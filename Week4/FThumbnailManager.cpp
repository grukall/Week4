#include "FThumbnailManager.h"
#include "FAssetManager.h"
#include "stb_image.h"
#include "FAABB.h"
#include "StaticMesh.h"
#include "Material.h"
#include "FEditorViewportClient.h"
#include "GraphicsManager.h"
#include "Renderer.h"
void FThumbnailManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, FAssetManager* assetManager, FGraphicsManager* graphicsManager)
{
    mDevice = device;
    mContext = context;
    mAssetManager = assetManager;
    mGraphicsManager = graphicsManager;

    CreateThumbnailRenderTarget(128, 128);
}

void FThumbnailManager::Shutdown() {
    ReleaseThumbnailRenderTarget();
    for (auto& pair : mThumbnailCache) {
        if (pair.second) pair.second->Release();
    }
    mThumbnailCache.clear();

    if (mFolderIconSRV) mFolderIconSRV->Release();
    if (mFileIconSRV) mFileIconSRV->Release();
}

ImTextureID FThumbnailManager::GetThumbnail(const std::filesystem::path& path, bool isDirectory)
{
    std::string pathStr = path.string();

    if (mThumbnailCache.find(pathStr) != mThumbnailCache.end())
    {
        return (ImTextureID)mThumbnailCache[pathStr];
    }

    if (isDirectory)
    {
        return (ImTextureID)mFolderIconSRV;
    }

    std::string ext = path.extension().string();
    ID3D11ShaderResourceView* srv = nullptr;

    if (ext == ".png" || ext == ".jpg")
    {
        srv = LoadTextureThumbnail(path);
    }
    else if (ext == ".uasset" || ext == ".obj")
    {
        // 3D 메시 썸네일 오프스크린 렌더링 호출!
        srv = LoadStaticMeshThumbnail(path);
    }

    if (!srv)
    {
        srv = mFileIconSRV;
    }

    mThumbnailCache[pathStr] = srv;
    return (ImTextureID)srv;
}

ID3D11ShaderResourceView* FThumbnailManager::LoadStaticMeshThumbnail(
    const std::filesystem::path& Path)
{
    constexpr UINT ThumbnailSize = 128;

    // ------------------------------------------------------------
    // 0. 필수 객체 확인
    // ------------------------------------------------------------
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

    // ------------------------------------------------------------
    // 1. StaticMesh Asset 가져오기
    // ------------------------------------------------------------
    const std::string AssetNameString = Path.stem().string();

    UStaticMesh* Mesh =
        mAssetManager->GetAssetAs<UStaticMesh>(
            FName(AssetNameString.c_str()),
            true);

    // 혹시 전체 경로를 AssetName으로 등록한 경우
    if (!Mesh)
    {
        const std::string FullPathString = Path.string();

        Mesh =
            mAssetManager->GetAssetAs<UStaticMesh>(
                FName(FullPathString.c_str()),
                true);
    }

    if (!Mesh)
    {
        return nullptr;
    }

    // ------------------------------------------------------------
    // 2. 렌더 가능한 메시인지 확인
    // ------------------------------------------------------------
    const TArray<FStaticMeshSection>& Sections = Mesh->GetSections();

    if (Sections.IsEmpty())
    {
        return nullptr;
    }

    if (!Mesh->GetIndexBuffer() || Mesh->GetIndexCount() == 0)
    {
        return nullptr;
    }

    // ------------------------------------------------------------
    // 3. 현재 D3D11 상태 백업
    // ------------------------------------------------------------
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

    // ------------------------------------------------------------
    // 4. RenderCollector 백업
    //
    //    썸네일을 그릴 때 현재 씬의 RenderInfo를 같이 렌더링하면
    //    안 되므로 잠시 비운다.
    // ------------------------------------------------------------
    FRenderCollector& RenderCollector =
        mGraphicsManager->GetRenderCollector();

    TArray<FRenderInfo> SavedRenderInfos =
        RenderCollector.RenderInfos;

    TArray<FRenderLineInfo> SavedLineInfos =
        RenderCollector.LineInfos;

    TArray<UPrimitiveComponent*> SavedPickTargets =
        RenderCollector.PickTargets;

    FCamera* SavedCamera =
        RenderCollector.Camera;

    // ------------------------------------------------------------
    // 복구 함수
    // ------------------------------------------------------------
    auto RestoreState = [&]()
        {
            // D3D11 상태 복구
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

            // RenderCollector 복구
            RenderCollector.RenderInfos =
                SavedRenderInfos;

            RenderCollector.LineInfos =
                SavedLineInfos;

            RenderCollector.PickTargets =
                SavedPickTargets;

            RenderCollector.Camera =
                SavedCamera;

            // OMGetRenderTargets가 잡은 AddRef 해제
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

    // ------------------------------------------------------------
    // 5. 썸네일용 카메라 생성
    //
    //    네 엔진 카메라가 +Z 방향을 Forward로 사용하는 구조이므로
    //    메시를 -Z 쪽에서 바라보게 한다.
    // ------------------------------------------------------------
    URenderer* Renderer =
        mGraphicsManager->GetRenderer();

    if (!Renderer)
    {
        RestoreState();
        return nullptr;
    }

    FEditorViewportClient ThumbnailViewport(*Renderer);

    ThumbnailViewport.mWidth = ThumbnailSize;
    ThumbnailViewport.mHeight = ThumbnailSize;

    ThumbnailViewport.mCamera.mFovDegree = 45.0f;

    // ------------------------------------------------------------
    // 6. AABB 기준으로 메시를 중앙 정렬
    // ------------------------------------------------------------
    const FAABB LocalBounds = Mesh->GetLocalBoundingBox();

    // GetLocalBoundingBox()의 Min/Max를 직접 활용
    FVector LocalMin = LocalBounds.Min;
    FVector LocalMax = LocalBounds.Max;

    FVector Center = (LocalMin + LocalMax) * 0.5f;
    FVector Extent = (LocalMax - LocalMin) * 0.5f;

    float Radius = std::sqrt(Extent.x * Extent.x + Extent.y * Extent.y + Extent.z * Extent.z);

    if (Radius < 0.001f || std::isnan(Radius))
    {
        Radius = 1.0f;
        Center = FVector(0.0f, 0.0f, 0.0f);
    }


    // 너무 가까우면 잘릴 수 있으므로 여유를 둔다.
    const float CameraDistance =
        Radius * 3.0f;

    // 기본 정면 방향
    ThumbnailViewport.mCamera.Transform.Rotation = FRotator(-20.0f, 45.0f, 0.0f);
    FVector Forward = ThumbnailViewport.mCamera.GetForwardVector();
    ThumbnailViewport.mCamera.Transform.Location = FVector(0.0f, 0.0f, 0.0f) - (Forward * CameraDistance);
    // ------------------------------------------------------------
    // 7. 메시의 중심을 원점으로 이동
    // ------------------------------------------------------------
    FTransform MeshTransform;
    MeshTransform.Location =
        FVector(
            -Center.x,
            -Center.y,
            -Center.z
        );

    MeshTransform.Rotation =
        FRotator(0.0f, 0.0f, 0.0f);

    MeshTransform.Scale =
        FVector(1.0f, 1.0f, 1.0f);

    const FMatrix WorldMatrix =
        MeshTransform.MakeMatrix();

    // ------------------------------------------------------------
    // 8. RenderCollector를 썸네일 전용 상태로 만든다.
    // ------------------------------------------------------------
    RenderCollector.RenderInfos.Empty();
    RenderCollector.LineInfos.Empty();
    RenderCollector.PickTargets.Empty();

    RenderCollector.Camera =
        &ThumbnailViewport.mCamera;

    // ------------------------------------------------------------
    // 9. Section마다 FRenderInfo 생성
    // ------------------------------------------------------------
    for (uint32 SectionIndex = 0;
        SectionIndex < Sections.Num();
        ++SectionIndex)
    {
        const FStaticMeshSection& Section =
            Sections[SectionIndex];

        // StaticMesh 자체의 Material Slot 사용
        UMaterial* Material =
            Mesh->GetMaterial(
                Section.MaterialSlotIndex);

        // Material이 없으면 DefaultMaterial 사용
        if (!Material)
        {
            Material =
                UMaterial::DefaultMaterial;
        }

        FRenderInfo RenderInfo{};

        RenderInfo.StaticMesh =
            Mesh;

        RenderInfo.Material =
            Material;

        RenderInfo.WorldTransformMatrix =
            WorldMatrix;

        RenderInfo.ObejctID =
            FObjectID{ 0, 0 };

        RenderInfo.SectionIndex =
            SectionIndex;

        RenderCollector.RenderInfos.Add(
            RenderInfo);
    }

    // ------------------------------------------------------------
    // 10. 썸네일 RenderTarget 바인딩
    // ------------------------------------------------------------
    mContext->OMSetRenderTargets(
        1,
        &mThumbRT.RTV,
        mThumbRT.DSV);

    D3D11_VIEWPORT ThumbnailViewportDesc = {};
    ThumbnailViewportDesc.TopLeftX = 0.0f;
    ThumbnailViewportDesc.TopLeftY = 0.0f;
    ThumbnailViewportDesc.Width =
        static_cast<float>(ThumbnailSize);
    ThumbnailViewportDesc.Height =
        static_cast<float>(ThumbnailSize);
    ThumbnailViewportDesc.MinDepth = 0.0f;
    ThumbnailViewportDesc.MaxDepth = 1.0f;

    mContext->RSSetViewports(
        1,
        &ThumbnailViewportDesc);

    // ------------------------------------------------------------
    // 11. 썸네일 RT 클리어
    // ------------------------------------------------------------
    const float ClearColor[4] = { 0.15f, 0.15f, 0.15f, 1.0f };
    mContext->ClearRenderTargetView(mThumbRT.RTV, ClearColor);

    mContext->ClearDepthStencilView(
        mThumbRT.DSV,
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
        1.0f,
        0);

    // ------------------------------------------------------------
    // 12. 기존 GraphicsManager 렌더 파이프라인 사용
    //
    //     네 엔진의 실제 순서:
    //     BindRenderTarget
    //     Prepare
    //     FlushLines
    //     Render
    // ------------------------------------------------------------
    mGraphicsManager->Prepare(
        &ThumbnailViewport.mCamera,
        static_cast<float>(ThumbnailSize),
        static_cast<float>(ThumbnailSize));
    RenderCollector.LineInfos.Empty();
    mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    bool bSavedGridFlag = FShowFlags::Get().IsEnabled(EShowFlag::Grid);
    FShowFlags::Get().SetEnabled(EShowFlag::Grid, false);

    mGraphicsManager->Render();

    FShowFlags::Get().SetEnabled(EShowFlag::Grid, bSavedGridFlag);
    // ------------------------------------------------------------
    // 13. 현재 썸네일 RT를 독립 Texture로 복사
    // ------------------------------------------------------------
    D3D11_TEXTURE2D_DESC TextureDesc = {};

    mThumbRT.Texture->GetDesc(
        &TextureDesc);

    Microsoft::WRL::ComPtr<ID3D11Texture2D>
        ThumbnailTexture;

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

    // GPU 리소스 복사
    mContext->CopyResource(
        ThumbnailTexture.Get(),
        mThumbRT.Texture);

    // ------------------------------------------------------------
    // 14. SRV 생성
    // ------------------------------------------------------------
    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};

    SRVDesc.Format =
        TextureDesc.Format;

    SRVDesc.ViewDimension =
        D3D11_SRV_DIMENSION_TEXTURE2D;

    SRVDesc.Texture2D.MostDetailedMip =
        0;

    SRVDesc.Texture2D.MipLevels =
        TextureDesc.MipLevels;

    Microsoft::WRL::ComPtr<
        ID3D11ShaderResourceView>
        ThumbnailSRV;

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

    // ------------------------------------------------------------
    // 15. 원래 상태 복구
    // ------------------------------------------------------------
    RestoreState();

    // ComPtr의 소유권을 호출자에게 넘긴다.
    return ThumbnailSRV.Detach();
}

ID3D11ShaderResourceView* FThumbnailManager::LoadTextureThumbnail(const std::filesystem::path& path) {
    int width, height, channels;
    unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (!data) return mFileIconSRV;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;
    initData.SysMemPitch = width * 4;

    ID3D11Texture2D* texture = nullptr;
    mDevice->CreateTexture2D(&desc, &initData, &texture);
    stbi_image_free(data);

    if (!texture) return mFileIconSRV;

    ID3D11ShaderResourceView* srv = nullptr;
    mDevice->CreateShaderResourceView(texture, nullptr, &srv);
    texture->Release();

    return srv;
}
void FThumbnailManager::CreateThumbnailRenderTarget(int width, int height)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    mDevice->CreateTexture2D(&desc, nullptr, &mThumbRT.Texture);
    mDevice->CreateRenderTargetView(mThumbRT.Texture, nullptr, &mThumbRT.RTV);
    mDevice->CreateShaderResourceView(mThumbRT.Texture, nullptr, &mThumbRT.SRV);

    D3D11_TEXTURE2D_DESC depthDesc = desc;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    mDevice->CreateTexture2D(&depthDesc, nullptr, &mThumbRT.DepthStencil);
    mDevice->CreateDepthStencilView(mThumbRT.DepthStencil, nullptr, &mThumbRT.DSV);
}

void FThumbnailManager::ReleaseThumbnailRenderTarget()
{
    if (mThumbRT.DSV) { mThumbRT.DSV->Release(); mThumbRT.DSV = nullptr; }
    if (mThumbRT.DepthStencil) { mThumbRT.DepthStencil->Release(); mThumbRT.DepthStencil = nullptr; }
    if (mThumbRT.SRV) { mThumbRT.SRV->Release(); mThumbRT.SRV = nullptr; }
    if (mThumbRT.RTV) { mThumbRT.RTV->Release(); mThumbRT.RTV = nullptr; }
    if (mThumbRT.Texture) { mThumbRT.Texture->Release(); mThumbRT.Texture = nullptr; }
}