#include "Common.hlsl"


struct VertexIn
{
    float3 PosL             : POSITION;
    float3 NormalL          : NORMAL;
    float2 TexC             : TEXCOORD;
    float3 TangentU         : TANGENT;
#ifdef SKINNED
    float3 BoneWeights      : WEIGHTS;
    uint4  BoneIndices      : BONEINDICES;
#endif
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
    
#ifdef SKINNED
    SkinVertex(vin.BoneWeights, vin.BoneIndices, vin.PosL, vin.NormalL, vin.TangentU);
#endif
    
    uint idx = gVisibleIndices[gIndexOffset + instanceID];
    float4x4 world = GetInstanceWorld(instanceID);                  // 내 인스턴스 행렬 골라 읽기
    
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
    
    pin.NormalW = normalize(pin.NormalW);
    pin.TangentW = normalize(pin.TangentW); // 이름은 VS 출력 멤버에 맞게
    // 노멀맵에서 노멀 읽어서 월드 공간으로
    float4 normalMapSample = gNormalMap.Sample(gsamLinear, pin.TexC);                                           // float4 -> rgb에서 rgba로
    float3 bumpNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.TangentW);
    
    float3 lightDir = normalize(-gLights[0].Direction);
    float ndotl = max(dot(bumpNormalW, lightDir), 0.0f);
   
    float shadowFactor = CalcShadowFactor(pin.ShadowPosH);
    
    float3 diffuse = shadowFactor * gLights[0].Strength * ndotl * diffuseAlbedo.rgb;
    // SV_POSITION은 PS에서 픽셀 좌표 -> 풀해상도 AO 맵을 그대로 Load
    // ao는 ambient에만 곱함
    // ao는 직사광이 아닌 간접광이 틈으로 덜 들어오는 것을 흉내내는 역할기 때문이다
    float ao = gSsaoMap.Load(int3(pin.PosH.xy, 0)).r;                                                    
    float3 ambient = ao * gAmbientLight.rgb * diffuseAlbedo.rgb;                                         // ambient, 환경 반사는 그대로 (그림자 안에서도 보여야함)
    
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
    if (gDebugSsao) return float4(ao, ao, ao, 1.0f);
    return float4(litColor, diffuseAlbedo.a);
}