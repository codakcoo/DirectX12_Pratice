#include "Common.hlsl"


struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentU : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
    VertexOut vout;
    float4x4 world = GetInstanceWorld(instanceID);
    float4 posW = mul(float4(vin.PosL, 1.0f), world);
    vout.NormalW = mul(vin.NormalL, (float3x3)world);
    vout.TangentW = mul(vin.TangentU, (float3x3)world);
    vout.PosH = mul(posW, gViewProj);
    vout.TexC = vin.TexC;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    // 노멀맵 요철까지 반영 (SSAO가 벽돌 줄눈에도 생기게)
    float3 nSample = gNormalMap.Sample(gsamLinear, pin.TexC).rgb;
    float3 bumpW = NormalSampleToWorldSpace(nSample, normalize(pin.NormalW), pin.TangentW);
    
    // 월드 -> 뷰 공간 (SSAO는 뷰 공간에서 계산)
    float3 normalV = mul(bumpW, (float3x3) gView);
    
    // [2단계 디버그] UNORM 백버퍼에 보이도록 [-1,1] -> [0,1]
    //return float4(normalV * 0.5f + 0.5f, 1.0f);
    // [3단계] float RT에 그대로 저장: 
    return float4(normalV, 0.0f);
}