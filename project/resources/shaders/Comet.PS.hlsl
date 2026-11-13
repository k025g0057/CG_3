#include "Object3d.hlsli"

struct Material
{
    float4 color;
    int enableLighting;
    float4x4 uvTransform;
};

ConstantBuffer<Material> gMaterial : register(b0);

float HashNoise(float3 p)
{
    return frac(sin(dot(p, float3(127.1f, 311.7f, 74.7f))) * 43758.5453f);
}

float ValueNoise(float3 p)
{
    float3 cell = floor(p);
    float3 local = frac(p);
    local = local * local * (3.0f - 2.0f * local);

    float n000 = HashNoise(cell + float3(0.0f, 0.0f, 0.0f));
    float n100 = HashNoise(cell + float3(1.0f, 0.0f, 0.0f));
    float n010 = HashNoise(cell + float3(0.0f, 1.0f, 0.0f));
    float n110 = HashNoise(cell + float3(1.0f, 1.0f, 0.0f));
    float n001 = HashNoise(cell + float3(0.0f, 0.0f, 1.0f));
    float n101 = HashNoise(cell + float3(1.0f, 0.0f, 1.0f));
    float n011 = HashNoise(cell + float3(0.0f, 1.0f, 1.0f));
    float n111 = HashNoise(cell + float3(1.0f, 1.0f, 1.0f));

    float x00 = lerp(n000, n100, local.x);
    float x10 = lerp(n010, n110, local.x);
    float x01 = lerp(n001, n101, local.x);
    float x11 = lerp(n011, n111, local.x);
    return lerp(lerp(x00, x10, local.y), lerp(x01, x11, local.y), local.z);
}

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float3 normal = normalize(input.normal);
    float3 rockCoord = normal * 5.5f;

    // 複数の大きさのノイズを重ね、岩肌の斑点と細かな粒状感を作る
    float coarse = ValueNoise(rockCoord);
    float medium = ValueNoise(rockCoord * 2.7f + 13.4f);
    float fine = ValueNoise(rockCoord * 8.0f + 31.7f);
    float stonePattern = coarse * 0.58f + medium * 0.30f + fine * 0.12f;

    float3 darkStone = float3(0.105f, 0.082f, 0.072f);
    float3 warmStone = float3(0.34f, 0.235f, 0.16f);
    float3 surfaceColor = lerp(darkStone, warmStone, smoothstep(0.28f, 0.76f, stonePattern));

    // 小さな窪みと少量の赤熱した割れ目を加える
    float pits = smoothstep(0.70f, 0.88f, fine);
    surfaceColor *= 1.0f - pits * 0.30f;
    float fissureNoise = ValueNoise(rockCoord * 4.1f + 57.2f);
    float fissure = (1.0f - smoothstep(0.015f, 0.065f, abs(fissureNoise - 0.48f)))
        * smoothstep(0.42f, 0.78f, coarse);
    surfaceColor = lerp(surfaceColor, float3(0.95f, 0.22f, 0.035f), fissure * 0.42f);

    // 法線に沿った陰影で、発光する球ではなく岩の塊として見せる
    float diffuse = saturate(dot(normal, normalize(float3(-0.42f, 0.58f, 0.70f))));
    float lighting = 0.30f + diffuse * 0.82f;
    float rim = pow(1.0f - saturate(abs(normal.z)), 2.0f);
    surfaceColor = surfaceColor * lighting + float3(0.045f, 0.28f, 0.72f) * rim * 0.72f;

    output.color = float4(surfaceColor * gMaterial.color.rgb, 1.0f);
    return output;
}
