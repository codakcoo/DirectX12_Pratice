StructuredBuffer<float> gInput : register(t0);          // 입력 (읽기 전용)
RWStructuredBuffer<float> gOutput : register(u0);        // 출력 (읽기/쓰기 = UAV)

[numthreads(64, 1, 1)]
void CS( uint3 dtid : SV_DispatchThreadID )
{
    gOutput[dtid.x] = gInput[dtid.x] * gInput[dtid.x];      // 제곱
}