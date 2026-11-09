struct VertexShaderOutput
{
    float32_t4 position : SV_POSITION;
    float32_t2 texcoord : TEXCOORD0; // 名札を「TEXCOORD0」に統一
    float32_t3 normal : NORMAL0; // 名札を「NORMAL0」に統一
};