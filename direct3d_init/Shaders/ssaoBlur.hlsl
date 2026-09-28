
cbuffer cbBlur : register(b0)				// 루트 상수 18 DWORD
{
    float4 gWeights[3];                      // cubffer에서 float[11]은 원소마다 16바이트라 float4로 묶음
    int gBlurRadius;
    uint gHorizontal;
    float2 gInvSize;                         // InvWidth + InvHeight
    float gProj22;                           // 깊이 복원용 (투영 행렬 두 원소만)
    float gProj32;                           // 깊이 복원용 (투영 행렬 두 원소만)
}

Texture2D gNormalMap        : register(t0);
Texture2D gDepthMap         : register(t1);
Texture2D gInputMap         : register(t2);

SamplerState gsamPointClamp     : register(s0);
SamplerState gsamDepthMap       : register(s1);

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    vout.TexC = float2((vid << 1) & 2, vid & 2);
    vout.PosH = float4(vout.TexC.x * 2.0f - 1.0f, 1.0f - vout.TexC.y * 2.0f, 0.0f, 1.0f);
    return vout;
}

float GetWeight(int i) { return gWeights[i >> 2][i & 3]; }
float NdcDepthToViewDepth(float z) { return gProj22 / (z - gProj22); }

float4 PS(VertexOut pin) : SV_TARGET
{
    float2 texOffset = gHorizontal ? float2(gInvSize.x, 0.0f) : float2(0.0f, gInvSize.y);
    
    float totalWeight = GetWeight(gBlurRadius);                     // 중심
    float4 color = totalWeight * gInputMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f);
    
    float3 centerN = gNormalMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f).xyz;
    float centerD = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamDepthMap, pin.TexC, 0.0f).r);
    
    [loop]
    for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
        if (i == 0) continue;
        float2 tex = pin.TexC + i * texOffset;
        
        float3 n = gNormalMap.SampleLevel(gsamPointClamp, tex, 0.0f).xyz;
        float d = NdcDepthToViewDepth(gDepthMap.SampleLevel(gsamDepthMap, tex, 0.0f).r);
        
        // 같은 면(노멀 비슷 + 깊이 가까움)일 때만 섞음 -> 경계 보존
        if (dot(n, centerN) >= 0.8f && abs(d - centerD) <= 0.2f)
        {
            float w = GetWeight(i + gBlurRadius);
            color += w * gInputMap.SampleLevel(gsamPointClamp, tex, 0.0f);
            totalWeight += w;
        }
    }
    
    return color / totalWeight;                     // 빠진 샘플만큼 재정규화
}