#include "Renderer.h"

constexpr uint32 MaxLineInstances = 1024;

namespace
{
	UINT GetByteSizeFromFormat(DXGI_FORMAT Format)
	{
		switch (Format)
		{
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			return 16;
		case DXGI_FORMAT_R32G32B32_FLOAT:
			return 12;
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
			return 8;
		case DXGI_FORMAT_R8G8B8A8_UNORM:
			return 4;
		default:
			return 0; // Unknown format
		}
	}
}

void URenderer::Create(HWND hWindow)
{
	CreateDeviceAndSwapChain(hWindow);
	CreateFrameBuffer();
	CreateDepthStencilBuffer();

	LineStructuredBuffer = CreateStructuredBuffer<FRenderLineInfo>(MaxLineInstances);

	LinePipeline = CreateRenderPipeline();
	LinePipeline->SetRasterRizerState(D3D11_CULL_NONE);
	LinePipeline->SetShader("Assets/Shaders/Line.hlsl");
	LinePipeline->AddConstantBuffer<FCameraConstants>();
	LinePipeline->SetShaderResource(0, LineStructuredBuffer->SRV);

	PrimitivePipeline = CreateRenderPipeline();
	PrimitivePipeline->SetRasterRizerState(D3D11_CULL_BACK, 0, {EViewModeIndex::VMI_Lit, EViewModeIndex::VMI_Wireframe});
	PrimitivePipeline->SetShader("Assets/Shaders/Mesh.hlsl");
	PrimitivePipeline->AddConstantBuffer<FConstants>();
	PrimitivePipeline->AddConstantBuffer<FMatrix>();
	PrimitivePipeline->SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);

	//staticMesh
	MeshPipeline = CreateRenderPipeline();
	MeshPipeline->SetRasterRizerState(D3D11_CULL_BACK, 0, { EViewModeIndex::VMI_Lit, EViewModeIndex::VMI_Wireframe });
	MeshPipeline->SetShader("Assets/shaders/StaticMeshShader.hlsl");
	MeshPipeline->AddConstantBuffer<FConstants>();
	MeshPipeline->AddConstantBuffer<FMatrix>();
	MeshPipeline->SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);

	StencilMarkPipeline = CreateRenderPipeline();
	StencilMarkPipeline->SetRasterRizerState(D3D11_CULL_BACK);
	StencilMarkPipeline->SetStencilState(false, false, D3D11_COMPARISON_ALWAYS, D3D11_STENCIL_OP_REPLACE, 1);
	StencilMarkPipeline->SetBlendState(ERenderBlendMode::NoColorWrite);
	StencilMarkPipeline->SetShader("Assets/Shaders/Mesh.hlsl");
	StencilMarkPipeline->AddConstantBuffer<FConstants>();
	StencilMarkPipeline->AddConstantBuffer<FMatrix>();

	StencilOutlinePipeline = CreateRenderPipeline();
	StencilOutlinePipeline->SetRasterRizerState(D3D11_CULL_BACK);
	StencilOutlinePipeline->SetStencilState(false, false, D3D11_COMPARISON_NOT_EQUAL, D3D11_STENCIL_OP_KEEP, 1);
	StencilOutlinePipeline->SetBlendState(ERenderBlendMode::Opaque);
	StencilOutlinePipeline->SetShader("Assets/Shaders/Mesh.hlsl");
	StencilOutlinePipeline->AddConstantBuffer<FConstants>();
	StencilOutlinePipeline->AddConstantBuffer<FMatrix>();

	Line2DPipeline = CreateRenderPipeline();
	Line2DPipeline->SetRasterRizerState(D3D11_CULL_NONE);
	Line2DPipeline->SetDepthStencilState(false, false);
	Line2DPipeline->SetShader("Assets/Shaders/Line2D.hlsl");
	Line2DPipeline->AddConstantBuffer<FLine2DConstants>();

	Circle2DPipeline = CreateRenderPipeline();
	Circle2DPipeline->SetRasterRizerState(D3D11_CULL_NONE);
	Circle2DPipeline->SetDepthStencilState(false, false);
	Circle2DPipeline->SetShader("Assets/Shaders/Circle2D.hlsl");
	Circle2DPipeline->AddConstantBuffer<FCircle2DConstants>();

	Triangle2DPipeline = CreateRenderPipeline();
	Triangle2DPipeline->SetRasterRizerState(D3D11_CULL_NONE);
	Triangle2DPipeline->SetDepthStencilState(false, false);
	Triangle2DPipeline->SetShader("Assets/Shaders/Triangle2D.hlsl");
	Triangle2DPipeline->AddConstantBuffer<FTriangle2DConstants>();

	WorldAxisPipeline = CreateRenderPipeline();
	WorldAxisPipeline->SetRasterRizerState(D3D11_CULL_NONE);
	WorldAxisPipeline->SetBlendState(ERenderBlendMode::Transparent);
	WorldAxisPipeline->SetShader("Assets/Shaders/WorldAxis.hlsl");
	WorldAxisPipeline->AddConstantBuffer<FWorldAxisConstants>();

	WorldGridPipeline = CreateRenderPipeline();
	WorldGridPipeline->SetRasterRizerState(D3D11_CULL_NONE);
	WorldGridPipeline->SetDepthStencilState(true, true);
	WorldGridPipeline->SetBlendState(ERenderBlendMode::Transparent);
	WorldGridPipeline->SetShader("Assets/Shaders/WorldGrid.hlsl");
	WorldGridPipeline->AddConstantBuffer<FWorldGridConstants>();

	QuadPipeline = CreateRenderPipeline();
	QuadPipeline->SetRasterRizerState(D3D11_CULL_NONE);
	QuadPipeline->SetDepthStencilState(false, true);
	QuadPipeline->SetBlendState(ERenderBlendMode::Transparent);
	QuadPipeline->SetShader("Assets/Shaders/Quad.hlsl");
	QuadPipeline->AddConstantBuffer<FQuadConstants>();
	QuadPipeline->AddConstantBuffer<FMatrix>();
	QuadPipeline->SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);

	Quad2DPipeline = CreateRenderPipeline();
	Quad2DPipeline->SetRasterRizerState(D3D11_CULL_NONE);
	Quad2DPipeline->SetDepthStencilState(false, false);
	Quad2DPipeline->SetBlendState(ERenderBlendMode::Transparent);
	Quad2DPipeline->SetShader("Assets/Shaders/Quad2D.hlsl");
	Quad2DPipeline->AddConstantBuffer<FQuadConstants>();
	Quad2DPipeline->AddConstantBuffer<FMatrix>();
	Quad2DPipeline->SetSamplerState(0, D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_WRAP, D3D11_TEXTURE_ADDRESS_WRAP);

	//GPU Time 측정을 위한 Query
	D3D11_QUERY_DESC DisjointDesc = {};
	DisjointDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;

	D3D11_QUERY_DESC StampDesc = {};
	StampDesc.Query = D3D11_QUERY_TIMESTAMP;

	for (uint32 i = 0; i < GpuTimerSlotCount; ++i)
	{
		Device->CreateQuery(&DisjointDesc, GpuTimers[i].Disjoint.GetAddressOf());
		Device->CreateQuery(&StampDesc, GpuTimers[i].StartStamp.GetAddressOf());
		Device->CreateQuery(&StampDesc, GpuTimers[i].EndStamp.GetAddressOf());
	}
}

bool URenderer::GetVideoMemoryInfo(uint64& OutUsed, uint64& OutBudget) const
{
	if (!DxgiAdapter) return false;

	DXGI_QUERY_VIDEO_MEMORY_INFO Info = {};
	if (FAILED(DxgiAdapter->QueryVideoMemoryInfo(
		0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &Info)))
	{
		return false;
	}

	OutUsed = Info.CurrentUsage;
	OutBudget = Info.Budget;
	return true;
}
void URenderer::CreateDeviceAndSwapChain(HWND hWindow)
{
	D3D_FEATURE_LEVEL FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0 };

	DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
	SwapChainDesc.BufferDesc.Width = 0;
	SwapChainDesc.BufferDesc.Height = 0;
	SwapChainDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	SwapChainDesc.SampleDesc.Count = 1;
	SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc.BufferCount = 2;
	SwapChainDesc.OutputWindow = hWindow;
	SwapChainDesc.Windowed = TRUE;
	SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	UINT CreateDeviceFlags = 0;

#if defined(_DEBUG)
	CreateDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
		nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT | CreateDeviceFlags,
		FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION,
		&SwapChainDesc, &SwapChain, &Device, nullptr, &DeviceContext);

	if (FAILED(hr) && (CreateDeviceFlags & D3D11_CREATE_DEVICE_DEBUG))
	{
		CreateDeviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
		hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE,
			nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT | CreateDeviceFlags,
			FeatureLevels, ARRAYSIZE(FeatureLevels), D3D11_SDK_VERSION,
			&SwapChainDesc, &SwapChain, &Device, nullptr, &DeviceContext);
	}

	if (FAILED(hr) || !SwapChain)
	{
		assert(SwapChain != nullptr);
		return;
	}

	SwapChain->GetDesc(&SwapChainDesc);
	Width = SwapChainDesc.BufferDesc.Width;
	Height = SwapChainDesc.BufferDesc.Height;
	ViewportInfo = { 0.0f, 0.0f, (float)Width, (float)Height, 0.0f, 1.0f };
	Projection2D = FMatrix::Ortho(0.0f, (float)Width, (float)Height, 0.0f, 0.0f, 1.0f);   // Left, Right, Bottom, Top, Near, Far

	//Adapter 가져오기
	//ID3D11Device → IDXGIDevice → IDXGIAdapter → IDXGIAdapter3
	Microsoft::WRL::ComPtr<IDXGIDevice> DxgiDevice;
	if (SUCCEEDED(Device->QueryInterface(IID_PPV_ARGS(&DxgiDevice))))
	{
		Microsoft::WRL::ComPtr<IDXGIAdapter> Adapter;
		if (SUCCEEDED(DxgiDevice->GetAdapter(&Adapter)))
		{
			Adapter.As(&DxgiAdapter);
		}
	}
}

void URenderer::ReleaseDeviceAndSwapChain()
{
	if (DeviceContext)
	{
		DeviceContext->Flush();
	}

	if (SwapChain)
	{
		SwapChain->Release();
		SwapChain = nullptr;
	}

	if (Device)
	{
		Device->Release();
		Device = nullptr;
	}

	if (DeviceContext)
	{
		DeviceContext->Release();
		DeviceContext = nullptr;
	}
}

void URenderer::CreateFrameBuffer()
{
	SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&FrameBuffer);

	D3D11_RENDER_TARGET_VIEW_DESC framebufferRTVdesc = {};
	framebufferRTVdesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	framebufferRTVdesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

	Device->CreateRenderTargetView(FrameBuffer, &framebufferRTVdesc, &FrameBufferRTV);
}

void URenderer::ReleaseFrameBuffer()
{
	if (FrameBufferRTV)
	{
		FrameBufferRTV->Release();
		FrameBufferRTV = nullptr;
	}

	if (FrameBuffer)
	{
		FrameBuffer->Release();
		FrameBuffer = nullptr;
	}
}

void URenderer::OnResize(UINT newWidth, UINT newHeight)
{
	if (newWidth == 0 || newHeight == 0)
	{
		return;
	}

	if (newWidth == Width && newHeight == Height)
	{
		return;
	}

	if (!SwapChain || !Device || !DeviceContext)
	{
		return;
	}

	// 1. Unbind render target
	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	// 2. Release swap chain backbuffer reference and RTV
	ReleaseFrameBuffer();

	// 3. Release depth stencil buffer & view
	if (DepthStencilView)
	{
		DepthStencilView->Release();
		DepthStencilView = nullptr;
	}
	if (DepthStencilBuffer)
	{
		DepthStencilBuffer->Release();
		DepthStencilBuffer = nullptr;
	}

	// 4. Resize swap chain buffers
	HRESULT hr = SwapChain->ResizeBuffers(0, newWidth, newHeight, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr))
	{
		return;
	}

	Width = newWidth;
	Height = newHeight;

	// 5. Recreate FrameBuffer RTV and DepthStencil
	CreateFrameBuffer();
	CreateDepthStencilBuffer();

	// 6. Update Viewport and 2D projection matrix
	ViewportInfo = { 0.0f, 0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 1.0f };
	Projection2D = FMatrix::Ortho(0.0f, static_cast<float>(Width), static_cast<float>(Height), 0.0f, 0.0f, 1.0f);
}

void URenderer::Release()
{
	DeviceContext->ClearState();

	WorldGridPipeline.reset();
	WorldAxisPipeline.reset();
	Triangle2DPipeline.reset();
	Circle2DPipeline.reset();
	Line2DPipeline.reset();
	PrimitivePipeline.reset();
	StencilMarkPipeline.reset();
	StencilOutlinePipeline.reset();

	for (auto& Pair : SamplerStatePool.SamplerStates)
	{
		Pair.second->Release();
	}
	SamplerStatePool.SamplerStates.Empty();

	for (auto& Pair : DepthStencilStatePool.DepthStencilStates)
	{
		Pair.second->Release();
	}
	DepthStencilStatePool.DepthStencilStates.Empty();

	for (auto& BlendState : BlendStatePool.BlendStates)
	{
		if (BlendState)
		{
			BlendState->Release();
			BlendState = nullptr;
		}
	}

	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
	DepthStencilView->Release();
	DepthStencilBuffer->Release();
	ReleaseFrameBuffer();
	ReleaseDeviceAndSwapChain();
}

void URenderer::SwapBuffer()
{
	if (bGpuTimerActive)
	{
		FGpuTimerSlot& Slot = GpuTimers[GpuTimerIndex];
		DeviceContext->End(Slot.EndStamp.Get());
		DeviceContext->End(Slot.Disjoint.Get());
		Slot.bInFlight = true;
		GpuTimerIndex = (GpuTimerIndex + 1) % GpuTimerSlotCount;
	}

	SET_CYCLE_COUNTER("GPU Time", LastGpuMs);   // 건너뛴 프레임도 이전 값 유지
	SwapChain->Present(0, 0);
}

void URenderer::Prepare(const FMatrix& ViewProjectionMatrix, const FMatrix& HUDProjection2D)
{
	FGpuTimerSlot& Slot = GpuTimers[GpuTimerIndex];

	// 아직 안 끝난 슬롯이면 이번 프레임은 재지 않는다. 인덱스도 그대로 둔다.
	bGpuTimerActive = !Slot.bInFlight || ResolveGpuTimer(Slot);

	if (bGpuTimerActive)
	{
		DeviceContext->Begin(Slot.Disjoint.Get());
		DeviceContext->End(Slot.StartStamp.Get());
	}

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DeviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);

	FCameraConstants CameraConstants;
	CameraConstants.ViewProjectionMatrix = ViewProjectionMatrix;
	CameraConstants.ViewportSize = FVector2((float)Width, (float)Height);

	LinePipeline->UpdateConstantBuffer(0, CameraConstants);
	PrimitivePipeline->UpdateConstantBuffer(1, ViewProjectionMatrix);
	StencilMarkPipeline->UpdateConstantBuffer(1, ViewProjectionMatrix);
	StencilOutlinePipeline->UpdateConstantBuffer(1, ViewProjectionMatrix);
	QuadPipeline->UpdateConstantBuffer(1, ViewProjectionMatrix);
	Quad2DPipeline->UpdateConstantBuffer(1, HUDProjection2D);
}

Microsoft::WRL::ComPtr<ID3D11Buffer> URenderer::CreateIndexBuffer(const uint32* Indices, UINT Count)
{
	D3D11_BUFFER_DESC IndexBufferDesc = {};
	IndexBufferDesc.ByteWidth = Count * sizeof(uint32);
	IndexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IndexBufferSRD = { Indices };
	
	Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer;
	Device->CreateBuffer(&IndexBufferDesc, &IndexBufferSRD, IndexBuffer.GetAddressOf());

	INC_MEMORY_STAT_BY("IndexBufferMem", IndexBufferDesc.ByteWidth);

	return IndexBuffer;
}

Microsoft::WRL::ComPtr<ID3D11Texture2D> URenderer::CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData)
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;

	if (InitialData)
	{
		D3D11_SUBRESOURCE_DATA TextureData = {};
		TextureData.pSysMem = InitialData;
		TextureData.SysMemPitch = Desc.Width * GetByteSizeFromFormat(Desc.Format);

		Device->CreateTexture2D(&Desc, &TextureData, &Texture);
	}
	else
	{
		Device->CreateTexture2D(&Desc, nullptr, &Texture);
	}

	// 밉맵은 계산하지 않고 0레벨만 센다.
	INC_MEMORY_STAT_BY("TextureMem",
		Desc.Width * Desc.Height * Desc.ArraySize * GetByteSizeFromFormat(Desc.Format));

	return Texture;
}

Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> URenderer::CreateShaderResourceView(Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture, const D3D11_SHADER_RESOURCE_VIEW_DESC* Desc)
{
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
	Device->CreateShaderResourceView(Texture.Get(), Desc, &SRV);
	return SRV;
}

TSharedPtr<FRenderPipeline> URenderer::CreateRenderPipeline()
{
	return MakeShared<FRenderPipeline>(Device, DeviceContext, &SamplerStatePool, &DepthStencilStatePool, &BlendStatePool);
}

TSharedPtr<FRenderTarget2D> URenderer::CreateRenderTarget2D(uint32 Width, uint32 Height, DXGI_FORMAT Format)
{
	TSharedPtr<FRenderTarget2D> RenderTarget = MakeShared<FRenderTarget2D>();

	D3D11_TEXTURE2D_DESC TextureDesc = {};
	TextureDesc.Width = Width;
	TextureDesc.Height = Height;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = Format;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	RenderTarget->Texture = CreateTexture2D(TextureDesc);

	D3D11_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	RTVDesc.Format = Format;
	RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	Device->CreateRenderTargetView(RenderTarget->Texture.Get(), &RTVDesc, RenderTarget->RTV.GetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	SRVDesc.Texture2D.MipLevels = 1;
	Device->CreateShaderResourceView(RenderTarget->Texture.Get(), &SRVDesc, RenderTarget->SRV.GetAddressOf());

	RenderTarget->Width = Width;
	RenderTarget->Height = Height;

	return RenderTarget;
}

TSharedPtr<FDepthStencil> URenderer::CreateDepthStencil(uint32 Width, uint32 Height)
{
	TSharedPtr<FDepthStencil> DepthStencil = MakeShared<FDepthStencil>();

	D3D11_TEXTURE2D_DESC TextureDesc = {};
	TextureDesc.Width = Width;
	TextureDesc.Height = Height;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	DepthStencil->Texture = CreateTexture2D(TextureDesc);

	D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
	DsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DsvDesc.Texture2D.MipSlice = 0;
	Device->CreateDepthStencilView(DepthStencil->Texture.Get(), &DsvDesc, DepthStencil->DSV.GetAddressOf());

	DepthStencil->Width = Width;
	DepthStencil->Height = Height;

	return DepthStencil;
}

void URenderer::BindPipeline(const TSharedPtr<FRenderPipeline>& Pipeline) const
{
	// RSSetState는 드로우 직전마다 갈아치워지므로 뷰 모드 선택은 여기서 해야 한다.
	// 이 모드를 지원하지 않는 파이프라인(2D/기즈모)은 Lit 상태로 폴백된다.
	DeviceContext->RSSetState(Pipeline->GetRasterizerState(ViewModeIndex));

	DeviceContext->OMSetDepthStencilState(Pipeline->DepthStencilState, Pipeline->StencilRef);
	DeviceContext->OMSetBlendState(Pipeline->BlendState, nullptr, 0xffffffff);
	DeviceContext->IASetPrimitiveTopology(Pipeline->PrimitiveTopology);
	DeviceContext->IASetInputLayout(Pipeline->InputLayout);
	DeviceContext->VSSetShader(Pipeline->VertexShader, nullptr, 0);
	DeviceContext->PSSetShader(Pipeline->PixelShader, nullptr, 0);
	
	if (Pipeline->ConstantBuffers.Num())
	{
		DeviceContext->VSSetConstantBuffers(0, Pipeline->ConstantBuffers.Num(), &Pipeline->ConstantBuffers[0]);
		DeviceContext->PSSetConstantBuffers(0, Pipeline->ConstantBuffers.Num(), &Pipeline->ConstantBuffers[0]);
	}
	else
	{
		DeviceContext->VSSetConstantBuffers(0, 0, nullptr);
		DeviceContext->PSSetConstantBuffers(0, 0, nullptr);
	}

	if (Pipeline->ShaderResourceViews.Num())
	{
		DeviceContext->VSSetShaderResources(0, Pipeline->ShaderResourceViews.Num(), &Pipeline->ShaderResourceViews[0]);
		DeviceContext->PSSetShaderResources(0, Pipeline->ShaderResourceViews.Num(), &Pipeline->ShaderResourceViews[0]);
	}
	else
	{
		DeviceContext->VSSetShaderResources(0, 0, nullptr);
		DeviceContext->PSSetShaderResources(0, 0, nullptr);
	}

	if (Pipeline->SamplerStates.Num())
	{
		DeviceContext->PSSetSamplers(0, Pipeline->SamplerStates.Num(), &Pipeline->SamplerStates[0]);
	}
	else
	{
		DeviceContext->PSSetSamplers(0, 0, nullptr);
	}
}

void URenderer::BindFrameBuffer()
{
	DeviceContext->OMSetRenderTargets(1, &FrameBufferRTV, nullptr);
	DeviceContext->RSSetViewports(1, &ViewportInfo);

	const float ClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	DeviceContext->ClearRenderTargetView(FrameBufferRTV, ClearColor);
}

void URenderer::BindRenderTarget(const TSharedPtr<FRenderTarget2D>& RenderTarget, const TSharedPtr<FDepthStencil>& DepthStencil, bool bClear)
{
	DeviceContext->OMSetRenderTargets(1, RenderTarget->RTV.GetAddressOf(), DepthStencil->DSV.Get());
	if (bClear)
	{
		DeviceContext->ClearRenderTargetView(RenderTarget->RTV.Get(), ClearColor);

		if (DepthStencil)
		{
			DeviceContext->ClearDepthStencilView(DepthStencil->DSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		}
	}

	D3D11_VIEWPORT Viewport = {};
	Viewport.TopLeftX = 0.0f;
	Viewport.TopLeftY = 0.0f;
	Viewport.Width = static_cast<float>(RenderTarget->Width);
	Viewport.Height = static_cast<float>(RenderTarget->Height);
	Viewport.MinDepth = 0.0f;
	Viewport.MaxDepth = 1.0f;

	DeviceContext->RSSetViewports(1, &Viewport);
}

void URenderer::RenderLines(const TArray<FRenderLineInfo>& Lines) const
{
	uint32 Remaining = Lines.Num();
	const FRenderLineInfo* Offset = Lines.Data();

	BindPipeline(LinePipeline);

	while (Remaining > 0)
	{
		uint32 BatchSize = FGenericPlatformMath::Min(Remaining, MaxLineInstances);
		LineStructuredBuffer->UpdateStructuredBuffer(Offset, BatchSize);

		UINT OffsetIndex = 0;
		DeviceContext->IASetVertexBuffers(0, 0, NULL, NULL, &OffsetIndex);
		DeviceContext->DrawInstanced(6, BatchSize, 0, 0);
		INC_DWORD_STAT("Draws");
		INC_DWORD_STAT_BY("Prims", BatchSize * 2);

		Remaining -= BatchSize;
		Offset += BatchSize;
	}
}

void URenderer::RenderHighlight(Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer, UINT NumVertices, Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer, UINT NumIndices, const FMatrix& Model, const FMatrix& OutlineModel, const FVector4& OutlineColor) const
{
	const bool bIndexed = IndexBuffer && NumIndices > 0;

	StencilMarkPipeline->UpdateConstantBuffer(0, FConstants{ Model, FVector4(1.f, 1.f, 1.f, 1.f), 0, 0 });
	if (bIndexed)
	{
		RenderPrimitiveIndexed(StencilMarkPipeline, VertexBuffer, IndexBuffer, NumIndices);
	}
	else
	{
		RenderPrimitive(StencilMarkPipeline, VertexBuffer, NumVertices);
	}

	StencilOutlinePipeline->UpdateConstantBuffer(0, FConstants{ OutlineModel, OutlineColor, 0, 0 });
	if (bIndexed)
	{
		RenderPrimitiveIndexed(StencilOutlinePipeline, VertexBuffer, IndexBuffer, NumIndices);
	}
	else
	{
		RenderPrimitive(StencilOutlinePipeline, VertexBuffer, NumVertices);
	}
}

void URenderer::RenderQuad(const FRenderQuadInfo& Info) const
{
	QuadPipeline->ClearShaderResource();
	
	if (Info.TextureSRV)
	{
		QuadPipeline->SetShaderResource(0, Info.TextureSRV);
	}

	QuadPipeline->SetBlendState(Info.BlendMode);
	QuadPipeline->SetDepthStencilState(Info.EnableDepthTest, Info.EnableDepthWrite);

	BindPipeline(QuadPipeline);

	D3D11_SHADER_RESOURCE_VIEW_DESC Desc{};
	Info.TextureSRV->GetDesc(&Desc);

	QuadPipeline->UpdateConstantBuffer(0, FQuadConstants{ Info.Model, Info.Color, Info.SubUV, Info.TextureSRV ? 1 : 0, Desc.Format == DXGI_FORMAT_R8_UNORM });

	UINT Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 0, NULL, NULL, &Offset);
	DeviceContext->Draw(6, 0);
	INC_DWORD_STAT("Draws");
	INC_DWORD_STAT_BY("Prims", 2);
}

void URenderer::RenderQuad2D(const FRenderQuadInfo& Info) const
{
	Quad2DPipeline->ClearShaderResource();

	bool bGrayscale = false;
	if (Info.TextureSRV)
	{
		Quad2DPipeline->SetShaderResource(0, Info.TextureSRV);

		D3D11_SHADER_RESOURCE_VIEW_DESC Desc{};
		Info.TextureSRV->GetDesc(&Desc);

		// 폰트 아틀라스는 R8이라 R 채널이 알파다.
		bGrayscale = (Desc.Format == DXGI_FORMAT_R8_UNORM);
	}

	Quad2DPipeline->SetBlendState(Info.BlendMode);

	BindPipeline(Quad2DPipeline);

	Quad2DPipeline->UpdateConstantBuffer(0, FQuadConstants{ Info.Model, Info.Color, Info.SubUV, Info.TextureSRV ? 1 : 0, bGrayscale });

	UINT Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 0, NULL, NULL, &Offset);
	DeviceContext->Draw(6, 0);
	INC_DWORD_STAT("Draws");
	INC_DWORD_STAT_BY("Prims", 2);
}

void URenderer::RenderPrimitive(const TSharedPtr<FRenderPipeline>& Pipeline, Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer, UINT NumVertices) const
{
	BindPipeline(Pipeline);

	UINT Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, Buffer.GetAddressOf(), &Pipeline->Stride, &Offset);
	DeviceContext->Draw(NumVertices, 0);
	INC_DWORD_STAT("Draws");
	INC_DWORD_STAT_BY("Prims", NumVertices / 3);
}


void URenderer::RenderPrimitive(Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer, UINT NumVertices, const FMatrix& Model) const
{
	PrimitivePipeline->UpdateConstantBuffer(0, FConstants{ Model, FVector4(1.0f, 1.0f, 1.0f, 1.0f), 1 });

	RenderPrimitive(PrimitivePipeline, Buffer, NumVertices);
}

void URenderer::RenderPrimitive(Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer, UINT NumVertices, const FMatrix& Model, const FVector4& Color) const
{
	PrimitivePipeline->UpdateConstantBuffer(0, FConstants{ Model, Color, 0 });

	RenderPrimitive(PrimitivePipeline, Buffer, NumVertices);
}

void URenderer::RenderPrimitiveIndexed(const TSharedPtr<FRenderPipeline>& Pipeline, Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer, Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer, UINT NumIndices, UINT StartIndex) const
{
	BindPipeline(Pipeline);

	UINT Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, VertexBuffer.GetAddressOf(), &Pipeline->Stride, &Offset);
	DeviceContext->IASetIndexBuffer(IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	DeviceContext->DrawIndexed(NumIndices, StartIndex, 0);
	INC_DWORD_STAT("Draws");
	INC_DWORD_STAT_BY("Prims", NumIndices / 3);
}

void URenderer::RenderPrimitiveIndexed(Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer, Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer, UINT NumIndices, const FMatrix& Model, UINT StartIndex) const
{
	PrimitivePipeline->UpdateConstantBuffer(0, FConstants{ Model, FVector4(1.0f, 1.0f, 1.0f, 1.0f), 1 });

	RenderPrimitiveIndexed(PrimitivePipeline, VertexBuffer, IndexBuffer, NumIndices, StartIndex);
}

void URenderer::RenderLine2D(const FVector2& Start, const FVector2& End, const FVector4& Color, float Thickness) const
{
	Line2DPipeline->UpdateConstantBuffer(0, FLine2DConstants{ Projection2D, Color, Start, End, Thickness });

	BindPipeline(Line2DPipeline);

	UINT Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 0, NULL, NULL, &Offset);
	DeviceContext->Draw(6, 0);
	INC_DWORD_STAT("Draws");
	INC_DWORD_STAT_BY("Prims", 2);
}

void URenderer::RenderCircle2D(const FVector2& Center, const FVector4& Color, float Radius) const
{
	Circle2DPipeline->UpdateConstantBuffer(0, FCircle2DConstants{ Projection2D, Color, Center, Radius });

	BindPipeline(Circle2DPipeline);

	UINT Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 0, NULL, NULL, &Offset);
	DeviceContext->Draw(6, 0);
	INC_DWORD_STAT("Draws");
	INC_DWORD_STAT_BY("Prims", 2);
}

void URenderer::RenderTriangle2D(const FVector2& Center, const FVector4& Color, float Size, float Rotation) const
{
	Triangle2DPipeline->UpdateConstantBuffer(0, FTriangle2DConstants{ Projection2D, Color, Center, Size, Rotation - PI * 0.5f });

	BindPipeline(Triangle2DPipeline);

	UINT Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 0, NULL, NULL, &Offset);
	DeviceContext->Draw(3, 0);
	INC_DWORD_STAT("Draws");
	INC_DWORD_STAT_BY("Prims", 1);
}

void URenderer::RenderWorldAxis(const FMatrix& View, const FMatrix& Projection, const FVector4& Color, const FVector& Axis, float Thickness) const
{
	// Use the scene viewport currently bound, which may differ from the window size.
	D3D11_VIEWPORT Viewport = {};
	UINT ViewportCount = 1;
	DeviceContext->RSGetViewports(&ViewportCount, &Viewport);
	WorldAxisPipeline->UpdateConstantBuffer(0, FWorldAxisConstants{
		View, Projection, Color, Axis, Thickness, FVector2(Viewport.Width, Viewport.Height) });

	BindPipeline(WorldAxisPipeline);

	UINT Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 0, NULL, NULL, &Offset);
	DeviceContext->Draw(6, 0);
	INC_DWORD_STAT("Draws");
	INC_DWORD_STAT_BY("Prims", 2);
}

void URenderer::RenderWorldGrid(const FMatrix& ViewProjection, const FVector& CameraLocation, float GridGap) const
{
	WorldGridPipeline->UpdateConstantBuffer(0, FWorldGridConstants{ ViewProjection, CameraLocation, GridGap });

	BindPipeline(WorldGridPipeline);

	UINT Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 0, NULL, NULL, &Offset);
	DeviceContext->Draw(6, 0);
	INC_DWORD_STAT("Draws");
	INC_DWORD_STAT_BY("Prims", 2);
}

//=============================================
void URenderer::CreateDepthStencilBuffer()
{
	D3D11_TEXTURE2D_DESC DepthTextureDesc = {};
	DepthTextureDesc.Width = Width;
	DepthTextureDesc.Height = Height;
	DepthTextureDesc.MipLevels = 1;
	DepthTextureDesc.ArraySize = 1;
	DepthTextureDesc.SampleDesc.Count = 1;
	DepthTextureDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DepthTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	Device->CreateTexture2D(&DepthTextureDesc, nullptr, &DepthStencilBuffer);

	D3D11_DEPTH_STENCIL_VIEW_DESC DsvDesc = {};
	DsvDesc.Format = DepthTextureDesc.Format;
	DsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;

	Device->CreateDepthStencilView(DepthStencilBuffer, &DsvDesc, &DepthStencilView);
}

bool URenderer::ResolveGpuTimer(FGpuTimerSlot& Slot)
{
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT DisjointData = {};

	// Disjoint의 End가 제일 마지막에 발행되므로, 얘가 준비됐으면 타임스탬프 둘도 준비됐다.
	const HRESULT Hr = DeviceContext->GetData(
		Slot.Disjoint.Get(), &DisjointData, sizeof(DisjointData),
		D3D11_ASYNC_GETDATA_DONOTFLUSH);

	if (Hr != S_OK)
	{
		return false;   // S_FALSE = 아직 GPU가 안 끝냄. bInFlight 유지하고 다음에 다시 시도
	}

	UINT64 StartTick = 0, EndTick = 0;
	DeviceContext->GetData(Slot.StartStamp.Get(), &StartTick, sizeof(StartTick), 0);
	DeviceContext->GetData(Slot.EndStamp.Get(), &EndTick, sizeof(EndTick), 0);

	// 구간 중 클럭이 바뀌었으면 두 틱의 기준이 달라 비교 불가. 버린다.
	if (!DisjointData.Disjoint && DisjointData.Frequency != 0)
	{
		LastGpuMs = (EndTick - StartTick) * 1000.0 / DisjointData.Frequency;
	}

	Slot.bInFlight = false;
	return true;
}