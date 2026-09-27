#include "Common.hlsl"

float4 VS(float3 PosL : POSITION, uint instanceID : SV_InstanceID) : SV_POSITION
{
    float4x4 world = GetInstanceWorld(instanceID);
    float4 posW = mul(float4(PosL, 1.0f), world);
    return mul(posW, gLightViewProj);
}