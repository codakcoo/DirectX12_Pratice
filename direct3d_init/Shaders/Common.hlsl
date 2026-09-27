#ifndef COMMON_HLSL
#define COMMON_HLSL

#define MaxLights 16

struct InstanceData
{
    float4x4 World;
};

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
};

// -- ·çÆ® ½Ã±×´ÏÃ³¿Í 1:1 --
Texture2D                               gDiffuseMap : register(t0);
StructuredBuffer<InstanceData>          gInstanceData : register(t1);
TextureCube                             gCubeMap : register(t2); // È¯°æ¸Ê
Texture2D                               gNormalMap : register(t3); // ³ë¸Ö¸Ê
Texture2D                               gShadowMap : register(t4); // ¼¨µµ¸Ê
StructuredBuffer<uint>                  gVisibleIndices : register(t5);

SamplerState                            gsamLinear : register(s0);
SamplerComparisonState                  gsamShadow : register(s1);

cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float4 gAmbientLight;
    Light gLights[MaxLights];
    
    float4x4 gLightViewProj;
    float4x4 gShadowTransform;
    float4x4 gView;                     // ºä °ø°£ ³ë¸ê¿ë
};

cbuffer cbView : register(b2)
{
    uint gIndexOffset;
};

// ºäº° ¸ñ·Ï -> Àü¿ª ÀÎ½ºÅÏ½º Çà·Ä
float4x4 GetInstanceWorld(uint instanceID)
{
    return gInstanceData[gVisibleIndices[gIndexOffset + instanceID]].World;
}

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
    // 0~1 -> -1~1
    float3 normalT = 2.0f * normalMapSample - 1.0f;
    
    // TBN ±âÀú ±¸¼º
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N); // ÅºÁ¨Æ®¸¦ ³ë¸Ö¿¡ Á÷±³È­ (±×¶÷-½´¹ÌÆ® Á÷±³È­; ÅºÁ¨Æ®¸¦ ³ë¸Ö¿¡ ¼öÁ÷ÀÌ µÇ°Ô º¸Á¤)
    float3 B = cross(N, T); // ¹ÙÀÌÅºÁ¨Æ® = ³ë¸Ö X ÅºÁ¨Æ®
    
    float3x3 TBN = float3x3(T, B, N);
    
    // ÅºÁ¨Æ® °ø°£ ³ë¸Ö -> ¿ùµå °ø°£
    return mul(normalT, TBN);
}

#endif