#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>

namespace uel {

    // ビルド時に Visual Studio の HLSL コンパイラ（fxc）が作ったシェーダ（.cso）を読み込んで、シェーダを作る。
    //   name はファイル名だけ（例："SkyboxVS"）。Data/Shader/SkyboxVS.cso を読む。
    //   .cso の出力先は Game.vcxproj の HLSL コンパイラ設定（Data\Shader\%(Filename).cso）と合わせてある。
    //   Shader/*.hlsl を直したら、ビルドすれば .cso も作り直される。
    // 見つからない・作れないときは、Visual Studio の「出力」に [Shader] から始まるメッセージを出して false を返す。

    // .cso の中身（シェーダのバイトコード）を読む
    bool LoadShaderBytecode(const char* name, std::vector<char>& out);

    // 頂点シェーダを作る。bytecode を渡すと、読んだバイトコードを返す（入力レイアウトを作るのに使う）
    bool CreateVertexShaderFromCso(ID3D11Device* device, const char* name,
        Microsoft::WRL::ComPtr<ID3D11VertexShader>& vs, std::vector<char>* bytecode = nullptr);

    // ピクセルシェーダを作る
    bool CreatePixelShaderFromCso(ID3D11Device* device, const char* name,
        Microsoft::WRL::ComPtr<ID3D11PixelShader>& ps);

} // namespace uel
