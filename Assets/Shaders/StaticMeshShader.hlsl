cbuffer modelConstants : register(b0) // FConstants
{
    row_major matrix Model;
    float4 Color;

    int UseVertexColor;
    int HasTexture;
    float2 UVScroll;
}

cbuffer viewConstants : register(b1) // FMatrix
{
    row_major matrix View;
}

cbuffer globalConstants : register(b2)
{
    float Time;
    float3 GlobalPadding;
}

struct VS_INPUT
{
    float4 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

Texture2D main_texture : register(t0);
SamplerState default_sampler : register(s0);


// Vertex Shader
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    output.position = mul(
		mul(input.position, Model),
		View
	);

    if (UseVertexColor != 0)
    {
        output.color = input.color;
    }
    else
    {
        output.color = Color;
    }

    output.uv = input.uv;

    return output;
}


// Pixel Shader
float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 final_color = input.color;

    if (HasTexture != 0)
    {
        float2 scrolledUV = input.uv + UVScroll;
        final_color *= main_texture.Sample(
			default_sampler,
			scrolledUV
		);
    }

    return final_color;
}