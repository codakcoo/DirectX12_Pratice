
TextureCube gCubeMap : register(t0);
SamplerState gsamLInear : register(s0);

cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
    float3 gEyePosW;
}

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float3 PosL : POSITION;             // 로컬 위치 = 큐브맵 샘플 방향
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // 큐브의 로컬 정점 위치가 곧 큐브맵을 샘플할 방향이다
    // 큐브 표면의 각 점이 바깥쪽 방향을 가리킴
    // 그 방향으로 큐브맵을 조회하면 그 방향의 하늘이 나옴.
    vout.PosL = vin.PosL;               // 로컬 위치를 그대로 샘플 방향으로
    
    // 스카이박스를 카메라 위치로 이동 (항상 카메라 중심)
    float3 posW = vin.PosL + gEyePosW;
    
    // z = w로 만들어서 깊이를 항상 1.0(최대)로
    // 클립 공간에서 z를 w로 바꾸면, 원근 나눗셈 후 z/w = w/w = 1.0이 됨.
    // 즉 깊이가 항상 최대(제일 뒤).
    // 그렇기에 다른 모든 물체가 스카이박스 앞에 그려짐.
    vout.PosH = mul(float4(posW, 1.0f), gViewProj).xyww;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    return gCubeMap.Sample(gsamLInear, pin.PosL);           // 방향으로 큐브맵 샘플
}