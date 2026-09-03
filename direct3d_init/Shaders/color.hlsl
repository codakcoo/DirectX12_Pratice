cbuffer cbPerObject : register(b0)
{
    float4x4 gWorldViewProj;
    //float4 gColor;
};

struct VertexIn
{
    float3 Pos : POSITION;
    float4 Color : COLOR;
};

struct VertexOut
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    // 로컬 공강 -> 클립 공간으로 변환
    vout.Pos = mul(float4(vin.Pos, 1.0f), gWorldViewProj);
    // 색상은 그대로 픽셀 셰이더로 전달
    vout.Color = vin.Color;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    // 픽셀 셰이더에서 색상을 그대로 출력
    return pin.Color;
}