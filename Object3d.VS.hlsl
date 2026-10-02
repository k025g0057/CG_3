#include "Object3d.hlsli"

// --- 追加：C++側から送られてくる行列データ（b1）を受け取る構造体 ---
struct TransformationMatrix
{
    float32_t4x4 wvp;
    float32_t4x4 World;
};

// register(b1) で、C++側の「1番目のスロット」と紐付けます
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);

struct VertexShaderInput
{
    float32_t4 position : POSITION;
    float32_t2 texcoord : TEXCOORD;
    float32_t3 normal : NORMAL;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // ★★★ 頂点の座標に行列を掛け算します
    // mul(座標, 行列) という関数を使って計算します
    output.position = mul(input.position, gTransformationMatrix.wvp);
    output.texcoord = input.texcoord;
    
    output.normal = normalize(mul(input.normal, (float32_t3x3) gTransformationMatrix.World));
    
    return output;
}