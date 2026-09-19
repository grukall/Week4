#pragma once

#include "Core.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "Matrix.h"
#include "Vector.h"
#include "RenderInfo.h"
#include "FRenderPipeline.h"
#include "FStatManager.h"

struct FCameraConstants
{
	FMatrix ViewProjectionMatrix;
	FVector2 ViewportSize;
	float Padding[2];
};

struct FConstants
{
	FMatrix Matrix;
	FVector4 Color;
	int32 UseVertexColor;
	int32 HasTexture;
	int32 Padding[2];
};

struct FLine2DConstants
{
	FMatrix Projection;
	FVector4 Color;
	FVector2 Start;
	FVector2 End;
	float Thickness;
	float Padding[3];
};

struct FCircle2DConstants
{
	FMatrix Projection;
	FVector4 Color;
	FVector2 Center;
	float Radius;
	float Padding[2];
};

struct FTriangle2DConstants
{
	FMatrix Projection;
	FVector4 Color;
	FVector2 Center;
	float Size;
	float Rotation;
};

struct FWorldAxisConstants
{
	FMatrix View;
	FMatrix Projection;
	FVector4 Color;
	FVector Axis;
	float Thickness;
	FVector2 ViewportSize;
	float Padding[2] = {};
};

struct FWorldGridConstants
{
	FMatrix ViewProjection;
	FVector CameraLocation;
	float GridGap = 1.0f;
};

struct FQuadConstants
{
	FMatrix Model;
	FVector4 Color;
	FVector4 SubUV;
	int32 HasTexture;
	int32 GrayscaleMode;
	int32 Padding[2];
};

struct FSamplerStateKey
{
	D3D11_FILTER Filter;
	D3D11_TEXTURE_ADDRESS_MODE AddressU;
	D3D11_TEXTURE_ADDRESS_MODE AddressV;

	bool operator==(const FSamplerStateKey& Other) const
	{
		return Filter == Other.Filter && AddressU == Other.AddressU && AddressV == Other.AddressV;
	}
};

struct FSamplerStateKeyHash
{
	std::size_t operator()(const FSamplerStateKey& Key) const
	{
		return std::hash<int>()(static_cast<int>(Key.Filter)) ^ (std::hash<int>()(static_cast<int>(Key.AddressU)) << 1) ^ (std::hash<int>()(static_cast<int>(Key.AddressV)) << 2);
	}
};

class FSamplerStatePool
{
public:
	ID3D11SamplerState* GetOrCreateSamplerState(ID3D11Device* Device, const FSamplerStateKey& Key)
	{
		ID3D11SamplerState** existing = SamplerStates.Find(Key);
		if (existing)
		{
			return *existing;
		}

		D3D11_SAMPLER_DESC SamplerDesc = {};
		SamplerDesc.Filter = Key.Filter;
		SamplerDesc.AddressU = Key.AddressU;
		SamplerDesc.AddressV = Key.AddressV;
		SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		SamplerDesc.MipLODBias = 0.0f;
		SamplerDesc.MaxAnisotropy = 1;
		SamplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		SamplerDesc.BorderColor[0] = 0.0f;
		SamplerDesc.BorderColor[1] = 0.0f;
		SamplerDesc.BorderColor[2] = 0.0f;
		SamplerDesc.BorderColor[3] = 0.0f;
		SamplerDesc.MinLOD = 0.0f;
		SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		ID3D11SamplerState* SamplerState = nullptr;
		HRESULT Hr = Device->CreateSamplerState(&SamplerDesc, &SamplerState);
		if (FAILED(Hr))
		{
			return nullptr;
		}

		SamplerStates.Add(Key, SamplerState);

		return SamplerState;
	}

private:
	friend class URenderer;

	TMap<FSamplerStateKey, ID3D11SamplerState*, FSamplerStateKeyHash> SamplerStates;
};

struct FDepthStencilStateKey
{
	bool bEnableDepthTest;
	bool bEnableDepthWrite;
	bool bEnableStencil = false;
	D3D11_COMPARISON_FUNC StencilFunc = D3D11_COMPARISON_ALWAYS;
	D3D11_STENCIL_OP StencilPassOp = D3D11_STENCIL_OP_KEEP;
	
	bool operator==(const FDepthStencilStateKey& Other) const
	{
		return bEnableDepthTest == Other.bEnableDepthTest
			&& bEnableDepthWrite == Other.bEnableDepthWrite
			&& bEnableStencil == Other.bEnableStencil
			&& StencilFunc == Other.StencilFunc
			&& StencilPassOp == Other.StencilPassOp;
	}
};

struct FDepthStencilStateKeyHash
{
	std::size_t operator()(const FDepthStencilStateKey& Key) const
	{
		return std::hash<bool>()(Key.bEnableDepthTest)
			^ (std::hash<bool>()(Key.bEnableDepthWrite) << 1)
			^ (std::hash<bool>()(Key.bEnableStencil) << 2)
			^ (std::hash<int>()(static_cast<int>(Key.StencilFunc)) << 3)
			^ (std::hash<int>()(static_cast<int>(Key.StencilPassOp)) << 5);
	}
};

class FDepthStencilStatePool
{
public:
	ID3D11DepthStencilState* GetOrCreateDepthStencilState(ID3D11Device* Device, const FDepthStencilStateKey& Key)
	{
		ID3D11DepthStencilState** existing = DepthStencilStates.Find(Key);
		if (existing)
		{
			return *existing;
		}

		D3D11_DEPTH_STENCIL_DESC DepthStencilDesc = {};
		DepthStencilDesc.DepthEnable = Key.bEnableDepthTest ? TRUE : FALSE;
		DepthStencilDesc.DepthWriteMask = Key.bEnableDepthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
		DepthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS;
		DepthStencilDesc.StencilEnable = Key.bEnableStencil ? TRUE : FALSE;
		DepthStencilDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		DepthStencilDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

		D3D11_DEPTH_STENCILOP_DESC StencilOpDesc = {};
		StencilOpDesc.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		StencilOpDesc.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		StencilOpDesc.StencilPassOp = Key.StencilPassOp;
		StencilOpDesc.StencilFunc = Key.StencilFunc;

		DepthStencilDesc.FrontFace = StencilOpDesc;
		DepthStencilDesc.BackFace = StencilOpDesc;

		ID3D11DepthStencilState* DepthStencilState = nullptr;
		HRESULT Hr = Device->CreateDepthStencilState(&DepthStencilDesc, &DepthStencilState);
		if (FAILED(Hr))
		{
			return nullptr;
		}

		DepthStencilStates.Add(Key, DepthStencilState);

		return DepthStencilState;
	}

private:
	friend class URenderer;

	TMap<FDepthStencilStateKey, ID3D11DepthStencilState*, FDepthStencilStateKeyHash> DepthStencilStates;
};

class FBlendStatePool
{
public:
	FBlendStatePool()
	{
		for (int i = 0; i < static_cast<int>(ERenderBlendMode::Count); ++i)
		{
			BlendStates[i] = nullptr;
		}
	}

	ID3D11BlendState* GetOrCreateBlendState(ID3D11Device* Device, ERenderBlendMode BlendMode)
	{
		ID3D11BlendState* Result = BlendStates[static_cast<int>(BlendMode)];
		if (Result)
		{
			return Result;
		}

		CD3D11_BLEND_DESC BlendDesc = {};
		BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		BlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		BlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		switch (BlendMode)
		{
		case ERenderBlendMode::Opaque:
		case ERenderBlendMode::Masked:
			BlendDesc.RenderTarget[0].BlendEnable = FALSE;
			break;
		case ERenderBlendMode::Transparent:
			BlendDesc.RenderTarget[0].BlendEnable = TRUE;
			BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
			BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			break;
		case ERenderBlendMode::Additive:
			BlendDesc.RenderTarget[0].BlendEnable = TRUE;
			BlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
			BlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
			BlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			break;
		case ERenderBlendMode::NoColorWrite:
			BlendDesc.RenderTarget[0].BlendEnable = FALSE;
			BlendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
			break;
		}

		ID3D11BlendState* BlendState = nullptr;
		HRESULT Hr = Device->CreateBlendState(&BlendDesc, &BlendState);
		if (FAILED(Hr))
		{
			return nullptr;
		}

		BlendStates[static_cast<int>(BlendMode)] = BlendState;

		return BlendState;
	}

private:
	friend class URenderer;

	ID3D11BlendState* BlendStates[static_cast<int>(ERenderBlendMode::Count)];
};

struct FRenderTarget2D
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> RTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
	UINT Width;
	UINT Height;
};

struct FDepthStencil
{
	Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> DSV;
	UINT Width;
	UINT Height;
};

struct FStructuredBuffer
{
	ID3D11DeviceContext* DeviceContext;

	Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> SRV;
	UINT ElementSize;
	UINT ElementCount;

	void UpdateStructuredBuffer(const void* Data, uint32 DataCount)
	{
		D3D11_BOX Box = {};
		Box.left = 0;
		Box.right = DataCount * ElementSize;
		Box.top = 0;
		Box.bottom = 1;
		Box.front = 0;
		Box.back = 1;

		DeviceContext->UpdateSubresource(Buffer.Get(), 0, &Box, Data, 0, 0);
	}
};

class URenderer
{
public:
	//create
	void Create(HWND hWindow);
	void Release();

#if 0
	void CreateLineVertexBuffer(uint32 maxVertices);

	void CreateStencilMarkState();
	void CreateStencilOutlineState();
	void CreateNoColorWriteBlendState();

	//release
	void ReleaseLineVertexBuffer();
#endif

	template <typename T>
	Microsoft::WRL::ComPtr<ID3D11Buffer> CreateVertexBuffer(T* Vertices, UINT Count)
	{
		D3D11_BUFFER_DESC VertexBufferDesc = {};
		VertexBufferDesc.ByteWidth = sizeof(T) * Count;
		VertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
		VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA VertexBufferSRD = { Vertices };

		Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer;
		Device->CreateBuffer(&VertexBufferDesc, &VertexBufferSRD, VertexBuffer.GetAddressOf());

		INC_MEMORY_STAT_BY("VertexBufferMem", VertexBufferDesc.ByteWidth);

		return VertexBuffer;
	}

	Microsoft::WRL::ComPtr<ID3D11Buffer> CreateIndexBuffer(const uint32* Indices, UINT Count);

	Microsoft::WRL::ComPtr<ID3D11Texture2D> CreateTexture2D(const D3D11_TEXTURE2D_DESC& Desc, const void* InitialData = nullptr);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> CreateShaderResourceView(Microsoft::WRL::ComPtr<ID3D11Texture2D> Texture, const D3D11_SHADER_RESOURCE_VIEW_DESC* Desc = nullptr);

	template <typename T>
	TSharedPtr<FStructuredBuffer> CreateStructuredBuffer(uint32 ElementCount)
	{
		TSharedPtr<FStructuredBuffer> StructuredBuffer = MakeShared<FStructuredBuffer>();
		StructuredBuffer->DeviceContext = DeviceContext;

		D3D11_BUFFER_DESC StructuredBufferDesc = {};
		StructuredBufferDesc.ByteWidth = sizeof(T) * ElementCount;
		StructuredBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		StructuredBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		StructuredBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		StructuredBufferDesc.StructureByteStride = sizeof(T);

		Device->CreateBuffer(&StructuredBufferDesc, nullptr, StructuredBuffer->Buffer.GetAddressOf());

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		SRVDesc.Buffer.FirstElement = 0;
		SRVDesc.Buffer.NumElements = ElementCount;

		Device->CreateShaderResourceView(StructuredBuffer->Buffer.Get(), &SRVDesc, StructuredBuffer->SRV.GetAddressOf());

		StructuredBuffer->ElementSize = sizeof(T);
		StructuredBuffer->ElementCount = ElementCount;

		return StructuredBuffer;
	}

	TSharedPtr<FRenderTarget2D> CreateRenderTarget2D(uint32 Width, uint32 Height, DXGI_FORMAT Format);
	TSharedPtr<FDepthStencil> CreateDepthStencil(uint32 Width, uint32 Height);
	
	//Update
	void RSUpdateState();

	//Rendering
	void Prepare(const FMatrix& ViewProjectionMatrix);
#if 0
	void RenderLines(const FVertexSimple* vertices, uint32 numVertices);
#endif

	TSharedPtr<FRenderPipeline> CreateRenderPipeline();

	void BindPipeline(const TSharedPtr<FRenderPipeline>& Pipeline) const;

	void BindFrameBuffer();
	void BindRenderTarget(const TSharedPtr<FRenderTarget2D>& RenderTarget, const TSharedPtr<FDepthStencil>& DepthStencil, bool bClear = true);

	void RenderLines(const TArray<FRenderLineInfo>& Lines) const;
	void RenderHighlight(Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer, UINT NumVertices, Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer, UINT NumIndices, const FMatrix& Model, const FMatrix& OutlineModel, const FVector4& OutlineColor) const;
	void RenderQuad(const FRenderQuadInfo& Info) const;
	void RenderQuad2D(const FRenderQuadInfo& Info) const;

	void RenderPrimitive(const TSharedPtr<FRenderPipeline>& Pipeline, Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer, UINT NumVertices) const;
	void RenderPrimitive(Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer, UINT NumVertices, const FMatrix& Model) const;
	void RenderPrimitive(Microsoft::WRL::ComPtr<ID3D11Buffer> Buffer, UINT NumVertices, const FMatrix& Model, const FVector4& Color) const;
	void RenderPrimitiveIndexed(const TSharedPtr<FRenderPipeline>& Pipeline, Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer, Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer, UINT NumIndices, UINT StartIndex = 0) const;
	void RenderPrimitiveIndexed(Microsoft::WRL::ComPtr<ID3D11Buffer> VertexBuffer, Microsoft::WRL::ComPtr<ID3D11Buffer> IndexBuffer, UINT NumIndices, const FMatrix& Model, UINT StartIndex = 0) const;

	void RenderLine2D(const FVector2& Start, const FVector2& End, const FVector4& Color, float Thickness = 1.0f) const;
	void RenderCircle2D(const FVector2& Center, const FVector4& Color, float Radius = 1.0f) const;
	void RenderTriangle2D(const FVector2& Center, const FVector4& Color, float Size = 1.0f, float Rotation = 0.0f) const;
	// Thickness is the full world-space width, matching the grid's 0.001 half-width.
	void RenderWorldAxis(const FMatrix& View, const FMatrix& Projection, const FVector4& Color, const FVector& Axis, float Thickness = 0.002f) const;
	void RenderWorldGrid(const FMatrix& ViewProjection, const FVector& CameraLocation, float GridGap) const;

	void SwapBuffer();

	//=============================================
	//해상도 변경 시 호출
	//void OnResize(UINT Width, UINT Height);
	void OnResize(UINT width, UINT height);

	FORCEINLINE uint32 GetWidth() const { return Width; }
	FORCEINLINE uint32 GetHeight() const { return Height; }
	FORCEINLINE const D3D11_VIEWPORT& GetViewport() const { return ViewportInfo; }
	FORCEINLINE ID3D11Device* GetDevice() const { return Device; }
	FORCEINLINE ID3D11DeviceContext* GetDeviceContext() const { return DeviceContext; }
	FORCEINLINE void SetViewModeIndex(EViewModeIndex InViewModeIndex) { ViewModeIndex = InViewModeIndex; }

private:
	void CreateDeviceAndSwapChain(HWND hWindow);
	void ReleaseDeviceAndSwapChain();

	void CreateFrameBuffer();
	void ReleaseFrameBuffer();

	void CreateDepthStencilBuffer();

private:
    ID3D11Device* Device = nullptr;
    ID3D11DeviceContext* DeviceContext = nullptr;
    IDXGISwapChain* SwapChain = nullptr;

	FSamplerStatePool SamplerStatePool;
	FDepthStencilStatePool DepthStencilStatePool;
	FBlendStatePool BlendStatePool;

    ID3D11Texture2D* FrameBuffer = nullptr;
    ID3D11RenderTargetView* FrameBufferRTV = nullptr;

	ID3D11Texture2D* DepthStencilBuffer = nullptr;			// 실제 깊이값이 저장될 메모리
	ID3D11DepthStencilView* DepthStencilView = nullptr;		// 그 메모리를 "출력 대상"으로 보는 뷰

	TSharedPtr<FStructuredBuffer> LineStructuredBuffer;

	TSharedPtr<FRenderPipeline> LinePipeline;
	TSharedPtr<FRenderPipeline> PrimitivePipeline;
	TSharedPtr<FRenderPipeline> StencilMarkPipeline;
	TSharedPtr<FRenderPipeline> StencilOutlinePipeline;
	TSharedPtr<FRenderPipeline> Line2DPipeline;
	TSharedPtr<FRenderPipeline> Circle2DPipeline;
	TSharedPtr<FRenderPipeline> Triangle2DPipeline;
	TSharedPtr<FRenderPipeline> WorldAxisPipeline;
	TSharedPtr<FRenderPipeline> WorldGridPipeline;
	TSharedPtr<FRenderPipeline> QuadPipeline;
	TSharedPtr<FRenderPipeline> Quad2DPipeline;

	UINT Width, Height;
    FLOAT ClearColor[4] = { 0.025f, 0.025f, 0.025f, 1.0f };
    D3D11_VIEWPORT ViewportInfo;
	FMatrix Projection2D;

	// 와이어프레임 여부. Prepare에서 갱신하고 BindPipeline이 읽는다.
	// RSSetState는 드로우 직전마다 덮어써지므로 플래그로 들고 있어야 한다.
	EViewModeIndex ViewModeIndex = EViewModeIndex::VMI_Lit;

#if 1
	ID3D11RasterizerState* RasterizerState[2] = {};
	ID3D11DepthStencilState* StencilMarkState = nullptr;	// 스텐실에 1 마킹용 상태
	ID3D11DepthStencilState* StencilOutlineState = nullptr; // 아웃라인 그리기용
	ID3D11BlendState* NoColorWriteBlendState = nullptr;		// 스텐실만 찍고 색은 쓰지 않는 상태

	// 매 프레임 내용이 바뀌는 선분용. 메시 버퍼와 달리 IMMUTABLE이 아니라 DYNAMIC이다
	ID3D11Buffer* LineVertexBuffer = nullptr;
	uint32 LineVertexCapacity = 0;
#endif
};
