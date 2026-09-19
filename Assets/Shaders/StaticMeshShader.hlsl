cbuffer modelConstants : register(b0)
{
    row_major matrix Model;

    float4 Color;

    float4 AmbientColor;
    float4 SpecularColor;
    float4 EmissiveColor;
    float4 TransmissionFilter;

    float SpecularPower;
    float OpticalDensity;
    float Transparency;
    uint IlluminationModel;

    uint UseVertexColor;
    uint HasTexture;

    float2 UVScroll;

    uint HasAmbientTexture;
    uint HasSpecularTexture;
    uint HasBumpTexture;
    uint Padding;
}

cbuffer viewConstants : register(b1)
{
    row_major matrix View;
}

cbuffer globalConstants : register(b2)
{
    float Time;
    float3 CameraPosition;
}


// ------------------------------------------------------------
// Vertex Input / Output
// ------------------------------------------------------------

struct VS_INPUT
{
    float4 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
    float3 worldPosition : TEXCOORD1;
};


// ------------------------------------------------------------
// Textures
// ------------------------------------------------------------

Texture2D ambient_texture : register(t0);
Texture2D diffuse_texture : register(t1);
Texture2D specular_texture : register(t2);
Texture2D bump_texture : register(t3);


// ------------------------------------------------------------
// Samplers
// ------------------------------------------------------------

SamplerState ambient_sampler : register(s0);
SamplerState diffuse_sampler : register(s1);
SamplerState specular_sampler : register(s2);
SamplerState bump_sampler : register(s3);


// ------------------------------------------------------------
// Vertex Shader
// ------------------------------------------------------------

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    // World Position
    float4 worldPosition = mul(input.position, Model);
    output.worldPosition = worldPosition.xyz;

    // View Position
    output.position = mul(worldPosition, View);

    // Color
    if (UseVertexColor != 0)
    {
        output.color = input.color;
    }
    else
    {
        output.color = Color;
    }

    // UV
    output.uv = input.uv;

    // Normal
    output.normal = normalize(mul(float4(input.normal, 0.0f), Model).xyz);

    return output;
}


// ------------------------------------------------------------
// Bump Normal
// ------------------------------------------------------------

float3 GetBumpNormal(PS_INPUT input, float3 baseNormal, float2 uv)
{
    if (HasBumpTexture == 0)
    {
        return baseNormal;
    }

    // --------------------------------------------------------
    // Texture Size
    // --------------------------------------------------------

    uint textureWidth;
    uint textureHeight;
    uint mipLevels;

    bump_texture.GetDimensions(0, textureWidth, textureHeight, mipLevels);

    float2 texelSize = 1.0f / float2(textureWidth, textureHeight);


    // --------------------------------------------------------
    // Height Samples
    // --------------------------------------------------------

    float heightCenter = bump_texture.Sample(bump_sampler, uv).r;
    float heightX = bump_texture.Sample(bump_sampler, uv + float2(texelSize.x, 0.0f)).r;
    float heightY = bump_texture.Sample(bump_sampler, uv + float2(0.0f, texelSize.y)).r;

    float dhdx = heightX - heightCenter;
    float dhdy = heightY - heightCenter;


    // --------------------------------------------------------
    // Derivatives
    // --------------------------------------------------------

    float3 dpdx = ddx(input.worldPosition);
    float3 dpdy = ddy(input.worldPosition);

    float2 duvdx = ddx(input.uv);
    float2 duvdy = ddy(input.uv);


    // --------------------------------------------------------
    // Tangent / Bitangent
    // --------------------------------------------------------

    float determinant = duvdx.x * duvdy.y - duvdx.y * duvdy.x;

    if (abs(determinant) < 0.000001f)
    {
        return baseNormal;
    }

    float inverseDeterminant = 1.0f / determinant;

    float3 tangent = (dpdx * duvdy.y - dpdy * duvdx.y) * inverseDeterminant;
    float3 bitangent = (dpdy * duvdx.x - dpdx * duvdy.x) * inverseDeterminant;


    // --------------------------------------------------------
    // Orthogonalize Tangent
    // --------------------------------------------------------

    tangent = normalize(tangent - baseNormal * dot(baseNormal, tangent));
    bitangent = normalize(cross(baseNormal, tangent));


    // --------------------------------------------------------
    // Bump Strength
    // --------------------------------------------------------

    const float BumpStrength = 1.0f;

    float3 bumpedNormal = normalize(baseNormal - tangent * dhdx * BumpStrength - bitangent * dhdy * BumpStrength);

    return bumpedNormal;
}


// ------------------------------------------------------------
// Pixel Shader
// ------------------------------------------------------------

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    // --------------------------------------------------------
    // UV
    // --------------------------------------------------------

    float2 uv = input.uv + UVScroll;


    // --------------------------------------------------------
    // Diffuse Color
    // --------------------------------------------------------

    float4 diffuseColor = input.color;

    // Diffuse Texture
    if (HasTexture != 0)
    {
        float4 textureColor = diffuse_texture.Sample(diffuse_sampler, uv);
        diffuseColor *= textureColor;
    }


    // --------------------------------------------------------
    // Ambient Color
    // --------------------------------------------------------

    float3 ambientColor = AmbientColor.rgb;

    if (HasAmbientTexture != 0)
    {
        float3 ambientTextureColor = ambient_texture.Sample(ambient_sampler, uv).rgb;
        ambientColor *= ambientTextureColor;
    }


    // --------------------------------------------------------
    // Specular Color
    // --------------------------------------------------------

    float3 specularColor = SpecularColor.rgb;

    if (HasSpecularTexture != 0)
    {
        float3 specularTextureColor = specular_texture.Sample(specular_sampler, uv).rgb;
        specularColor *= specularTextureColor;
    }


    // --------------------------------------------------------
    // Normal
    // --------------------------------------------------------

    float3 normal = normalize(input.normal);
    normal = GetBumpNormal(input, normal, uv);


    // --------------------------------------------------------
    // Light Direction
    // --------------------------------------------------------

    float3 lightDirection = normalize(float3(-0.5f, -1.0f, -0.5f));
    float3 lightColor = float3(1.0f, 1.0f, 1.0f);


    // --------------------------------------------------------
    // View Direction
    // --------------------------------------------------------

    float3 viewDirection = normalize(CameraPosition - input.worldPosition);


    // --------------------------------------------------------
    // Diffuse Lighting
    // --------------------------------------------------------

    float diffuseFactor = saturate(dot(normal, -lightDirection));
    float3 diffuseLighting = diffuseColor.rgb * diffuseFactor * lightColor;


    // --------------------------------------------------------
    // Specular Lighting
    // --------------------------------------------------------

    float3 halfVector = normalize(-lightDirection + viewDirection);
    float specularFactor = pow(saturate(dot(normal, halfVector)), max(SpecularPower, 1.0f));
    float3 specularLighting = specularColor * specularFactor * lightColor;


    // --------------------------------------------------------
    // Ambient Lighting
    // --------------------------------------------------------

    float3 ambientLighting = ambientColor * diffuseColor.rgb;


    // --------------------------------------------------------
    // Fresnel
    // OpticalDensity = IOR
    // --------------------------------------------------------

    float ior = max(OpticalDensity, 1.0f);
    float f0 = pow((1.0f - ior) / (1.0f + ior), 2.0f);
    float viewDotNormal = saturate(dot(normal, viewDirection));
    float fresnel = f0 + (1.0f - f0) * pow(1.0f - viewDotNormal, 5.0f);


    // --------------------------------------------------------
    // Illumination Model
    // --------------------------------------------------------

    float3 result = float3(0.0f, 0.0f, 0.0f);

    switch (IlluminationModel)
    {
        // ----------------------------------------------------
        // illum 0
        // Diffuse color only
        // ----------------------------------------------------
        case 0:
        {
                result = diffuseColor.rgb;
                break;
            }

        // ----------------------------------------------------
        // illum 1
        // Ambient + Diffuse
        // ----------------------------------------------------
        case 1:
        {
                result = ambientLighting + diffuseLighting;
                break;
            }

        // ----------------------------------------------------
        // illum 2+
        // Ambient + Diffuse + Specular
        // ----------------------------------------------------
        default:
        {
                result = ambientLighting + diffuseLighting + specularLighting;
                break;
            }
    }


    // --------------------------------------------------------
    // Fresnel Specular
    // --------------------------------------------------------

    result += specularLighting * fresnel;


    // --------------------------------------------------------
    // Emissive
    // --------------------------------------------------------

    result += EmissiveColor.rgb;


    // --------------------------------------------------------
    // Transmission
    // --------------------------------------------------------

    float transmissionAmount = 1.0f - saturate(Transparency);

    if (transmissionAmount > 0.0f)
    {
        result = lerp(result, result * TransmissionFilter.rgb, transmissionAmount * (1.0f - fresnel));
    }


    // --------------------------------------------------------
    // Alpha
    //
    // MTL:
    // d  = opacity
    // Tr = 1 - opacity
    // --------------------------------------------------------

    float alpha = diffuseColor.a * saturate(Transparency);

    return float4(result, alpha);
}