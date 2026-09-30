#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
#ifdef SKINNED
    float3 BoneWeights : WEIGHTS;
    uint4 BoneIndices : BONEINDICES;
#endif
};

float4 VS(VertexIn vin, uint instanceID : SV_InstanceID) : SV_POSITION
{
#ifdef SKINNED
    float3 n = 0.0f, t = 0.0f;                      // 섀도는 위치만 필요 (노멀/탄젠트는 컴파일러가 제거)
    SkinVertex(vin.BoneWeights, vin.BoneIndices, vin.PosL, n, t);
#endif
    float4x4 world = GetInstanceWorld(instanceID);
    float4 posW = mul(float4(vin.PosL, 1.0f), world);
    return mul(posW, gLightViewProj);
}