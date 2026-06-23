// ==========================================
// PrimitiveRenderer 共通ヘッダー
// ==========================================

struct VS_IN
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD; // ★追加：テクスチャのUV座標
};

struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD; // ★追加：ピクセルシェーダーへ送るUV座標
};

cbuffer CbScene : register(b0)
{
    row_major float4x4 viewProjection;
    int useTexture; // ★追加：テクスチャを使用するかどうかのフラグ
    float3 pad; // 16バイトアライメント用のパディング
};