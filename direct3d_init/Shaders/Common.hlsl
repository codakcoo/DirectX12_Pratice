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

// -- 루트 시그니처와 1:1 --
Texture2D                               gDiffuseMap : register(t0);
StructuredBuffer<InstanceData>          gInstanceData : register(t1);
TextureCube                             gCubeMap : register(t2); // 환경맵
Texture2D                               gNormalMap : register(t3); // 노멀맵
Texture2D                               gShadowMap : register(t4); // 섀도맵
StructuredBuffer<uint>                  gVisibleIndices : register(t5);
Texture2D                               gSsaoMap : register(t6);

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
    float4x4 gView;                     // 뷰 공간 노멸용
};

cbuffer cbView : register(b2)
{
    uint gIndexOffset;
    uint gDebugSsao;                    // 1이면 AO만 출력
};

cbuffer cSkinned : register(b3)
{
    float4x4 gBoneTransforms[96];
}

#ifdef SKINNED
// 가중치 4개(4번째 = 1 - 합)로 본 행렬을 섞음 (선형 블렌드 스키닝)
void SkinVertex(float3 w3, uint4 idx, inout float3 posL, inout float3 normalL, inout float3 tangentL)
{
    float w[4] = { w3.x, w3.y, w3.z, 1.0f - w3.x - w3.y - w3.z };

    float3 p = 0.0f, n = 0.0f, t = 0.0f;
    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float4x4 M = gBoneTransforms[idx[i]];
        p += w[i] * mul(float4(posL, 1.0f), M).xyz;
        n += w[i] * mul(normalL,  (float3x3)M);             // 본 행렬에 비균등 스케일 없음 가정
        t += w[i] * mul(tangentL, (float3x3)M);
    }
    posL = p; normalL = n; tangentL = t;
}
#endif

// 뷰별 목록 -> 전역 인스턴스 행렬
float4x4 GetInstanceWorld(uint instanceID)
{
    return gInstanceData[gVisibleIndices[gIndexOffset + instanceID]].World;
}

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
    // 0~1 -> -1~1
    float3 normalT = 2.0f * normalMapSample - 1.0f;
    
    // TBN 기저 구성
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N); // 탄젠트를 노멀에 직교화 (그람-슈미트 직교화; 탄젠트를 노멀에 수직이 되게 보정)
    float3 B = cross(N, T); // 바이탄젠트 = 노멀 X 탄젠트
    
    float3x3 TBN = float3x3(T, B, N);
    
    // 탄젠트 공간 노멀 -> 월드 공간
    return mul(normalT, TBN);
}

#endif