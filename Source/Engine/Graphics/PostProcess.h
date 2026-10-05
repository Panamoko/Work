#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "Engine/Graphics/PostProcessSettings.h"

namespace uel {
    class RenderTexture;

    // ポストプロセス：シーンを描き終えた画像を読み、画面全体に後処理を掛けて別の描画先へ書き出す。
    // 画面を覆う三角形を1枚描き、ピクセルシェーダで元の画像を1ピクセルずつ読んで加工する。
    class PostProcess {
    public:
        bool Initialize(ID3D11Device* device);

        // scene（シーンを描いた先。色・法線・深度）を読んで、dest に書き出す。
        // scene と dest は別のものであること（同じものを読みながら書くことはできない）。
        // proj はシーンを描いたときの射影行列（深度を距離に戻すのに使う）。
        void Apply(ID3D11DeviceContext* context, const RenderTexture& scene, RenderTexture& dest,
            const PostProcessSettings& settings, const DirectX::XMFLOAT4X4& proj);

    private:
        Microsoft::WRL::ComPtr<ID3D11VertexShader>      vs;
        Microsoft::WRL::ComPtr<ID3D11PixelShader>       ps;
        Microsoft::WRL::ComPtr<ID3D11Buffer>            cb;
        Microsoft::WRL::ComPtr<ID3D11SamplerState>      sampler;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> noDepth;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState>   raster;
    };

} // namespace uel