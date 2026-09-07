#define MaxLights 16

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
};

cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    //float4 gColor;
};

cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
    float3 gEyePosW;
    float cbPerObjectPad1;
    float4 gAmbientLight;
    Light gLights[MaxLights];
};

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITIONT;
    float3 NormalW : NORMAL;
};

VertexOut VS(VertexIn vin)
{
    VertexOut vout;
    
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    
    // 노멀은 월드 공간으로 변환 (비균등 스케일 없으니 World 그대로 사용 가능)
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    
    vout.PosH = mul(posW, gViewProj);
    
    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    float3 normal = normalize(pin.NormalW);
    float3 toEye = normalize(gEyePosW - pin.PosW);
    
    // 매우 단순화한 디렉셔널 라이트 (Lambert 확산 반사만)
    float3 lightDir = normalize(-gLights[0].Direction);
    float3 ndotl = max(dot(normal, lightDir), 0.0f);
    
    float3 diffuse = gLights[0].Strength * ndotl;
    float3 ambient = gAmbientLight.rgb;
    
    float3 litColor = ambient + diffuse;
    
    return float4(litColor, 1.0f);
}