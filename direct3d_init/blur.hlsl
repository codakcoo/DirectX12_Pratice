cbuffer cbBlurSettings : register(b0)
{
    int gBlurRadius;
    float w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10;      // 가우시안 가중치 (최대 반경 5)
};

static const int gMaxBlurRadius = 5;

Texture2D gInput : register(t0);
// 2D 텍스처를 UAV로 사용.
// gOutput[int2(x,y)] 처럼 2D 좌표로 접근하고, 각 픽셀이 RGBA(float4)임
RWTexture2D<float4> gOutput : register(u0);

#define N 256
#define CacheSize (N + 2 * gMaxBlurRadius)
// 스레드 그룹이 공유하는 캐시.
// 256개 스레드가 각자 이웃을 전역 메모리에서 읽으면 중복이 엄청남
// 캐시에 한 번만 읽어두고 재사용. CUDA __shared__ 그대로임
groupshared float4 gCache[CacheSize];

[numthreads(N, 1, 1)]
void HorzBlurCS(int3 groupThreadID : SV_GroupThreadID,
                int3 dispatchThreadID : SV_DispatchThreadID)
{
    float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };
    
    // 텍스처 크기 구하기
    uint width, height;
    gInput.GetDimensions(width, height);
    
    // 그룹 경계 밖 픽셀도 캐시에 채워야 하므로, 양 끝 스레드가 추가로 읽음
    if(groupThreadID.x < gBlurRadius)
    {
        int x = max(dispatchThreadID.x - gBlurRadius, 0);
        gCache[groupThreadID.x] = gInput[int2(x, dispatchThreadID.y)];
    }
    if(groupThreadID.x >= N - gBlurRadius)
    {
        int x = min(dispatchThreadID.x + gBlurRadius, width - 1);
        gCache[groupThreadID.x + 2 * gBlurRadius] = gInput[int2(x, dispatchThreadID.y)];
    }
    
    // 자기 픽셀은 항상 캐시에 (경계 클램프)
    gCache[groupThreadID.x + gBlurRadius] = gInput[min(dispatchThreadID.xy, int2(width - 1, height - 1))];
    
    // 그룹 내 모든 스레드가 캐시 채우기를 끝낼 때까지 대기 (장벽)
    // 안하면 어떤 스레드가 아직 안 채운 캐시를 다른 스레드가 읽어서 쓰레기 값이 나옴
    // CUDA __syncthreads() 랑 동일
    GroupMemoryBarrierWithGroupSync();
    
    // 캐시에서 이웃을 읽어 가중 평균
    float4 blurColor = float4(0, 0, 0, 0);
    for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
        int k = groupThreadID.x + gBlurRadius + i;
        blurColor += weights[i + gBlurRadius] * gCache[k];
    }

    
    gOutput[dispatchThreadID.xy] = blurColor;
}

[numthreads[1, N, 1]]
void VertBlurCS(int3 groupThreadID : SV_GroupThreadID,
                int3 dispatchThreadID : SV_DispatchThreadID)
{
    float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };

    uint width, height;
    gInput.GetDimensions(width, height);
    
    if (groupThreadID.y < gBlurRadius)
    {
        int y = max(dispatchThreadID.y - gBlurRadius, 0);
        gCache[groupThreadID.y] = gInput[int2(dispatchThreadID.x, y)];
    }
    if(groupThreadID.y >= N - gBlurRadius)
    {
        int y = min(dispatchThreadID.y + gBlurRadius, height - 1);
        gCache[groupThreadID.y + 2 * gBlurRadius] = gInput[int2(dispatchThreadID.x, y)];
    }
    
    gCache[groupThreadID.y + gBlurRadius] = gInput[min(dispatchThreadID.xy, int2(width - 1, height - 1))];
    
    GroupMemoryBarrierWithGroupSync();
    
    float4 blurColor = float4(0, 0, 0, 0);
    for (int i = -gBlurRadius; i <= gBlurRadius; ++i)
    {
        int k = groupThreadID.y + gBlurRadius + i;
        blurColor += weights[i + gBlurRadius] * gCache[k];
    }
    
    gOutput[dispatchThreadID.xy] = blurColor;

}