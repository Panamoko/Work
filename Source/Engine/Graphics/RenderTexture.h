#pragma once
#include <d3d11.h>
#include <wrl/client.h>

namespace uel {

    // 描画先になるテクスチャ。用途で2種類ある。
    //   forScene = true  : シーンを描く先。色に加えて、法線の書き出し先と深度を持つ。
    //                      どれも後からテクスチャとして読める（ポストプロセスで使う）。
    //   forScene = false : 色だけ（ポストプロセスの書き出し先など）。深度は持たない。
    class RenderTexture {
    public:
        bool Create(ID3D11Device* device, int width, int height, bool forScene);
        void Resize(ID3D11Device* device, int width, int height);   // 種類は Create のときのまま

        // 色（と、あれば深度）を描画先にしてクリアする。法線の書き出し先もクリアするが、描画先にはしない。
        void Begin(ID3D11DeviceContext* context, float r, float g, float b, float a);

        // forScene のときだけ：色＋法線の2枚を描画先にする（深度はそのまま）。
        // 法線を書き出さないもの（空）を描き終えてから呼ぶ。
        void BindSceneTargets(ID3D11DeviceContext* context);

        ID3D11ShaderResourceView* GetSRV()       const { return srv.Get(); }
        ID3D11ShaderResourceView* GetNormalSRV() const { return normalSRV.Get(); }   // forScene のときだけ有効
        ID3D11ShaderResourceView* GetDepthSRV()  const { return depthSRV.Get(); }    // forScene のときだけ有効
        int GetWidth()  const { return width; }
        int GetHeight() const { return height; }

    private:
        Microsoft::WRL::ComPtr<ID3D11Texture2D>          texture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   rtv;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;

        // 法線（rgb = ワールド法線を 0〜1 に詰めたもの、a = 1 なら物がある／0 なら空）
        Microsoft::WRL::ComPtr<ID3D11Texture2D>          normalTex;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>   normalRTV;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalSRV;

        Microsoft::WRL::ComPtr<ID3D11Texture2D>          depthTex;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView>   dsv;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthSRV;

        int  width = 0;
        int  height = 0;
        bool forScene = false;
    };

} // namespace uel