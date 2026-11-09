#include "Particle.hlsli"

// --- 追加：C++側から送られてくる行列データ（b1）を受け取る構造体 ---
struct TransformationMatrix
{
    float4x4 wvp;
    float4x4 World;
};

StructuredBuffer<TransformationMatrix> gTransformationMatrices : register(t0);

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    uint instanceId : SV_InstanceID;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
   // ★ input.instanceId に修正
    output.position = mul(input.position, gTransformationMatrices[input.instanceId].wvp);
    output.texcoord = input.texcoord;
    
    // ★ input.instanceId に修正
    output.normal = normalize(
        mul(input.normal, (float3x3)gTransformationMatrices[input.instanceId].World)
    );
    return output;
}
