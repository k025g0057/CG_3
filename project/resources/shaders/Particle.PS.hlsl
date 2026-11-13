#include "Particle.hlsli"

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // 粒子中心は明るく、外側は広く柔らかく光らせる
    float2 p = (input.texcoord - 0.5f) * 2.0f;
    float instanceSeed = (float)input.instanceId * 0.173f;
    float angle = atan2(p.y, p.x);
    float wobble = 0.07f * sin(angle * 5.0f + instanceSeed)
        + 0.035f * sin(angle * 9.0f - instanceSeed * 1.7f);
    float radius = length(float2(p.x * 0.88f, p.y)) / (1.0f + wobble);
    float core = 1.0f - smoothstep(0.035f, 0.50f, radius);
    float halo = 1.0f - smoothstep(0.20f, 1.0f, radius);
    float alpha = saturate(core * 0.78f + halo * 0.40f);

    // 尾のインスタンス番号を0～1にし、先端からの位置として色に使う
    float tailAmount = saturate(((float)input.instanceId - 32.0f) / 608.0f);
    uint track = input.instanceId >= 32 ? (input.instanceId - 32) % 6 : 0;
    float3 cyan = float3(0.12f, 0.92f, 1.0f);
    float3 blue = float3(0.12f, 0.36f, 1.0f);
    float3 violet = float3(0.48f, 0.16f, 0.92f);
    // tailAmountに応じて水色→青→紫へグラデーションさせる
    float3 color = lerp(cyan, blue, smoothstep(0.08f, 0.58f, tailAmount));
    color = lerp(color, violet, smoothstep(0.64f, 1.0f, tailAmount) * 0.80f);

    // 6本の筋に少しずつ色と明るさの差をつける
    if (track == 1 || track == 4)
    {
        color = lerp(color, float3(0.40f, 0.82f, 1.0f), 0.32f);
    }
    else if (track == 2 || track == 5)
    {
        color = lerp(color, float3(0.58f, 0.36f, 1.0f), 0.25f);
    }

    // 先頭の粒子は白く強く光り、青い核を作る
    if (input.instanceId < 32)
    {
        color = lerp(float3(0.05f, 0.50f, 1.0f), float3(0.60f, 0.94f, 1.0f), core);
        color = lerp(color, float3(1.0f, 1.0f, 1.0f), core * 0.88f);
    }
    else
    {
        color = lerp(color, float3(0.86f, 0.98f, 1.0f), core * 0.70f);
    }

    // 核は強く光らせ、尾は後方に行くほど明るさを落とす
    float brightness = input.instanceId < 32 ? 3.0f : lerp(2.4f, 0.85f, tailAmount);
    output.color = float4(color * alpha * brightness, alpha);
    return output;
}
