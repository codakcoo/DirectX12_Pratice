#define MaxLights 16

struct InstanceData
{
    float4x4 World;
};
StructuredBuffer<InstanceData> gInstanceData : register(t1);
StructuredBuffer<uint> gVisibleIndices : register(t5);

struct Light
{
    float3 Strength;
    float FallofStart;
    float3 Direction;
    float FallofEnd;
    float3 Position;
    float SpotPower;
};

// color.hlsl과 레이아웃 완전히 동일해야 함
cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float4 gAmbientLight;
    Light gLights[MaxLights];
    
    float4x4 gLightViewProj;
    float4x4 gShadowTransform;
};

cbuffer cbView : register(b2)
{
    uint gIndexOffset;
};

float4 VS(float3 PosL : POSITION, uint instanceID : SV_InstanceID) : SV_POSITION
{
    float4x4 world = gInstanceData[instanceID].World;
    float4 posW = mul(float4(PosL, 1.0f), world);
    return mul(posW, gLightViewProj);
}