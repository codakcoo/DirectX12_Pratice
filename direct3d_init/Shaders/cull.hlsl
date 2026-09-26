struct InstanceData
{
    float4x4 World;
};
StructuredBuffer<InstanceData>  gInstanceData   : register(t0);
RWStructuredBuffer<uint>        gCulledIndices  : register(u0);
RWByteAddressBuffer             gDrawArgs       : register(u1);         // D3D12_DRAW_INDEXED_ARGUMENTS

cbuffer cbCull : register(b0)
{
    float4 gPlanes[6];              // 월드 공간 프러스텀 평면 (안쪽이 +)
    uint gInstanceCount;
}

[numthreads(64, 1, 1)]
void CullCS(uint3 id : SV_DispatchThreadID)
{
    uint i = id.x;
    if (i >= gInstanceCount)
        return;
    
    // CPU에서 전치해서 올렸으므로 HLSL에서 원래 행렬 그대로 -> 3행의 이동값
    float3 center = gInstanceData[i].World[3].xyz;
    const float radius = 1.7320508f;                        // +- 큐브의 경계구 반지름 = 루트3
    
    [unroll]
    for (int p = 0; p < 6; ++p)
    {
        if (dot(gPlanes[p].xyz, center) + gPlanes[p].w < -radius)
            return;                                                     // 한 평면이라도 완전히 밖이면 컬링
    }

    uint slot;
    gDrawArgs.InterlockedAdd(4, 1, slot);                               // byte offset 4 = InstanceCount
    gCulledIndices[slot] = i;
}