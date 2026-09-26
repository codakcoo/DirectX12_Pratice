#define MaxLights 16

/*
* 1. 카메라에서 표면으로 향하는 시선 벡터
* 2. 표면 노멀로 그 시선을 반사시킨 벡터
* 3. 그 반사 벡터 방향으로 큐브맵(하늘)을 조회 -> 그 방향의 하늘이 표면에 비침
*    reflect(시선, 노멀)이 반사 벡터를 계산함
*/

struct InstanceData
{
    float4x4 World;
};


Texture2D gDiffuseMap                       : register(t0);
TextureCube gCubeMap                        : register(t2);                    // 환경맵
Texture2D gNormalMap                        : register(t3);                    // 노멀맵
Texture2D gShadowMap                        : register(t4);                    // 섀도맵
StructuredBuffer<uint> gVisibleIndices      : register(t5);
SamplerState gsamLinear                     : register(s0);
SamplerComparisonState gsamShadow           : register(s1);

StructuredBuffer<InstanceData> gInstanceData : register(t1);


struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
};

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

struct VertexIn
{
    float3 PosL     : POSITION;
    float3 NormalL  : NORMAL;
    float2 TexC     : TEXCOORD;
    float3 TangentU : TANGENT;
};

struct VertexOut
{
    float4 PosH         : SV_POSITION;
    float3 PosW         : POSITION;
    float3 NormalW      : NORMAL;
    float3 TangentW     : TANGENT;
    float2 TexC         : TEXCOORD;
    float4 ShadowPosH   : POSITION1;      // 섀도맵 UV 공간 좌표
};

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
    // 0~1 -> -1~1
    float3 normalT = 2.0f * normalMapSample - 1.0f;
    
    // TBN 기저 구성
    float3 N = unitNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);              // 탄젠트를 노멀에 직교화 (그람-슈미트 직교화; 탄젠트를 노멀에 수직이 되게 보정)
    float3 B = cross(N, T);                                             // 바이탄젠트 = 노멀 X 탄젠트
    
    float3x3 TBN = float3x3(T, B, N);
    
    // 탄젠트 공간 노멀 -> 월드 공간
    return mul(normalT, TBN);
}

float CalcShadowFactor(float4 shadowPosH)
{
    shadowPosH.xyz /= shadowPosH.w;                                 // 직교 투영이면 w=1이지만 습관적으로
    float depth = shadowPosH.z;                                     // 라이트 기준 내 깊이
    
    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);
    float dx = 1.0f / (float) width;                                // 텍셀 1칸
    
    const float2 offsets[9] =
    {
        float2(-dx, -dx),   float2(0.0f, -dx),  float2(dx, -dx),
        float2(-dx, 0.0f),  float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(-dx, +dx),   float2(0.0f, +dx),  float2(dx, +dx)
    };

    float percentLit = 0.0f;
    [unroll]
    for (int i = 0; i < 9; i++)
        percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow, shadowPosH.xy + offsets[i], depth).r;
    
    return percentLit / 9.0f;
}

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
    VertexOut vout;
    
    uint idx = gVisibleIndices[gIndexOffset + instanceID];
    float4x4 world = gInstanceData[idx].World; // 내 인스턴스 행렬 골라 읽기
    
    float4 posW = mul(float4(vin.PosL, 1.0f), world);
    vout.PosW = posW.xyz;
    // 노멀은 월드 공간으로 변환 (비균등 스케일 없으니 World 그대로 사용 가능)
    vout.NormalW = mul(vin.NormalL, (float3x3) world);
    vout.TangentW = mul(vin.TangentU, (float3x3) world);
    vout.PosH = mul(posW, gViewProj);
    vout.TexC = vin.TexC;
    vout.ShadowPosH = mul(posW, gShadowTransform);
    
    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamLinear, pin.TexC);
    
    // 노멀맵에서 노멀 읽어서 월드 공간으로
    float4 normalMapSample = gNormalMap.Sample(gsamLinear, pin.TexC);                                           // float4 -> rgb에서 rgba로
    float3 bumpNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, normalize(pin.NormalW), pin.TangentW);
    
    float3 lightDir = normalize(-gLights[0].Direction);
    float ndotl = max(dot(bumpNormalW, lightDir), 0.0f);
   
    float shadowFactor = CalcShadowFactor(pin.ShadowPosH);
    
    float3 diffuse = shadowFactor * gLights[0].Strength * ndotl * diffuseAlbedo.rgb;
    float3 ambient = gAmbientLight.rgb * diffuseAlbedo.rgb;                                         // ambient, 환경 반사는 그대로 (그림자 안에서도 보여야함)
    
    // --스페큘러(Blinn-Phong)--
    float3 toEyeW = normalize(gEyePosW - pin.PosW);     // 표면 -> 카메라
    float3 halfVec = normalize(lightDir + toEyeW);      // 하프 벡터

    const float shininess = 64.0f;                      // 확인용으로 넓게 64->16
    float spec = pow(max(dot(bumpNormalW, halfVec), 0.0f), shininess);
    spec *= (ndotl > 0.0f);                             // 빛 반대쪽 면에는 하이라이트 없음
    
    float glossMask = normalMapSample.a;
    float3 specular = shadowFactor * gLights[0].Strength * spec * glossMask;

    float3 litColor = ambient + diffuse + specular;

    // -- 환경 반사--
    float3 r = reflect(-toEyeW, bumpNormalW);           // 카메라->표면 방향으로 넣어야 함
    litColor += 0.3f * glossMask * gCubeMap.Sample(gsamLinear, r).rgb;      // 반사율 30%
    
    //return float4(bumpNormalW * 0.5f + 0.5f, 1.0f);           // 노멀 시각화
    //return float4(glossMask.xxx, 1.0f);                       // 마스크 시각화
    //return float4(specular, 1.0f);                            // 하이라트 시각화 
    //return float4(shadowFactor.xxx, 1.0f);                    // 섀도우 시각화
    return float4(litColor, diffuseAlbedo.a);
}