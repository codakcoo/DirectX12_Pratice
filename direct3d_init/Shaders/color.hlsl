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


Texture2D gDiffuseMap : register(t0);
TextureCube gCubeMap : register(t2);                    // 환경맵
SamplerState gsamLinear : register(s0);

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
};

struct VertexIn
{
    float3 PosL     : POSITION;
    float3 NormalL  : NORMAL;
    float2 TexC     : TEXCOORD;
};

struct VertexOut
{
    float4 PosH     : SV_POSITION;
    float3 PosW     : POSITION;
    float3 NormalW  : NORMAL;
    float2 TexC     : TEXCOORD;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
    VertexOut vout;
    
    float4x4 world = gInstanceData[instanceID].World; // 내 인스턴스 행렬 골라 읽기
    
    float4 posW = mul(float4(vin.PosL, 1.0f), world);
    vout.PosW = posW.xyz;
    // 노멀은 월드 공간으로 변환 (비균등 스케일 없으니 World 그대로 사용 가능)
    vout.NormalW = mul(vin.NormalL, (float3x3) world);
    vout.PosH = mul(posW, gViewProj);
    vout.TexC = vin.TexC;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_TARGET
{
    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamLinear, pin.TexC);
    
    float3 normal = normalize(pin.NormalW);
    // 매우 단순화한 디렉셔널 라이트 (Lambert 확산 반사만)
    float3 lightDir = normalize(-gLights[0].Direction);
    float ndotl = max(dot(normal, lightDir), 0.0f);
    
    float3 diffuse = gLights[0].Strength * ndotl * diffuseAlbedo.rgb;
    float3 ambient = gAmbientLight.rgb * diffuseAlbedo.rgb;
    float3 litColor = ambient + diffuse;
    
    // --환경 반사--
    float3 toEye = normalize(pin.PosW - gEyePosW); // 카메라 -> 표면 방향
    float3 reflectVec = reflect(toEye, normal);
    float4 reflectionColor = gCubeMap.Sample(gsamLinear, reflectVec);
    
    // 반사를 섞음 (30%)
    litColor += 0.3f * reflectionColor.rgb;
    
    return float4(litColor, diffuseAlbedo.a);

}