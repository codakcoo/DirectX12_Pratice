cbuffer cbSsao : register(b0)				// 루트 상수 52 DWORD
{
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gProjTex;                      // 뷰 -> 텍스처 UV
    float gOcclusionRadius;
    float gOcclusionFadeStart;
    float gOcclusionFadeEnd;
    float gSurfaceEpsilon;
};

Texture2D gNormalMap : register(t0);        // 뷰 공간 노멀 (SRV 슬롯 4)
Texture2D gDepthMap : register(t1);         // NDC 깊이 (SRV 슬롯 5)

SamplerState gsamPointClamp : register(s0);
SamplerState gsamDepthMap : register(s1);           // 화면 밖 = 깊이 1(먼 곳)

// 큐브 꼭짓점 8 + 면 중심 6 = 고르게 퍼진 14방향, 길이는 제각각
static const float3 gOffsetDirs[14] =
{
    float3(+1, +1, +1), float3(-1, -1, -1), float3(-1, +1, +1), float3(+1, -1, -1),
    float3(+1, +1, -1), float3(-1, -1, +1), float3(-1, +1, -1), float3(+1, -1, +1),
    float3(-1,  0,  0), float3(+1,  0,  0), float3( 0, -1,  0), float3( 0, +1,  0),
    float3( 0,  0, -1), float3( 0,  0, +1)
};
static const float gOffsetLens[14] =
{
    0.25f, 0.90f, 0.50f, 0.70f, 0.35f, 1.00f, 0.60f,
    0.30f, 0.80f, 0.45f, 0.95f, 0.40f, 0.55f, 0.75f
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float3 PosV : POSITION;                     // 근평면 위의 뷰 공간 점 (픽셀로 가는 광선)
    float2 TexC : TEXCOORD;
};

// 정점 버퍼 없이 화면을 덮는 삼각형 1개
VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    vout.TexC = float2((vid << 1) & 2, vid & 2);                            // (0,0) (2,0) (0,2)
    vout.PosH = float4(vout.TexC.x * 2.0f - 1.0f, 1.0f - vout.TexC.y * 2.0f, 0.0f, 1.0f);
    
    float4 ph = mul(vout.PosH, gInvProj);
    vout.PosV = ph.xyz / ph.w;
    return vout;
}

// NDC 깊이 -> 뷰 공간 z (투영 행렬 역산)
float NdcDepthToViewDepth(float zNdc)
{
    return gProj[3][2] / (zNdc - gProj[2][2]);
}

// 픽셀마다 다른 랜덤 단위 벡터 (샘플 패턴 회전용)
float3 RandomVec(float2 pixel)
{
    float3 v = frac(sin(float3(dot(pixel, float2(12.9898f, 78.233f)),
                               dot(pixel, float2(39.3460f, 11.135f)),
                               dot(pixel, float2(73.1560f, 52.235f)))) * 43758.5453f);
    
    return normalize(v * 2.0f - 1.0f);
}

// 가린점 r이 p보다 얼마나 앞에 있는가 -> 가림 정도
float OcclusionFunction(float distZ)
{
    float occlusion = 0.0f;
    if (distZ > gSurfaceEpsilon)                                // 같은 면(자기 자신)은 제외
    {
        float fadeLength = gOcclusionFadeEnd - gOcclusionFadeStart;
        occlusion = saturate((gOcclusionFadeEnd - distZ) / fadeLength);         // 너무 멀면 영향 없음
    }
    return occlusion;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    // 1. 이 픽셀의 뷰 공간 위치 p와 노멀 n 복원
    float3 n = normalize(gNormalMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f).xyz);
    float pz = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamDepthMap, pin.TexC, 0.0f).r);
    float3 p = (pz / pin.PosV.z) * pin.PosV;
    
    float3 randVec = RandomVec(pin.PosH.xy);
    
    // 2. 반구 안의 점 q를 14개 뽑아 가려졌는지 검사
    float occlusionSum = 0.0f;
    [unroll]
    for (int i = 0; i < 14; ++i)
    {
        float3 offset = reflect(normalize(gOffsetDirs[i]) * gOffsetLens[i], randVec);
        float flip = sign(dot(offset, n));                                              // 노멀 쪽 반구로 뒤집기
        float3 q = p + flip * gOcclusionRadius * offset;
        
        // q를 화면에 투영 -> 그 위치에서 실제로 보이는 표면 r
        float4 projQ = mul(float4(q, 1.0f), gProjTex);
        projQ /= projQ.w;
        float rz = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamDepthMap, projQ.xy, 0.0f).r);
        float3 r = (rz / q.z) * q;
        
        // r이 p 앞에 있고, n 방향 쪽에 있을수록 많이 가림
        float distZ = p.z - r.z;
        float dp = max(dot(n, normalize(r - p)), 0.0f);
        occlusionSum += dp * OcclusionFunction(distZ);
    }
    
    float access = 1.0f - occlusionSum / 14.0f;
    float ao = saturate(pow(access, 6.0f));                             // 대비 강화
    
    return float4(ao, ao, ao, 1.0f);                        // 디버그: 회색조로 바로 출력
}