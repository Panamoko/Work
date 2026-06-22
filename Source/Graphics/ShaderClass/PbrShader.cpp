#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include "Core/Misc.h"
#include "Graphics/GpuResourceUtils.h"
#include "Graphics/ShaderClass/PbrShader.h"

// ============================================================
// HLSL を「起動時に直接コンパイル」するヘルパー。
// cso ファイル（事前コンパイル済み）を使わないので、
// vcxproj の出力先設定やフォルダ名のズレに一切影響されません。
// ============================================================
static Microsoft::WRL::ComPtr<ID3DBlob> CompileShaderFromFile(
    const wchar_t* filename,   // 例: L"Shader/PbrPS.hlsl" または L"Data/Shader/PbrPS.hlsl"
    const char* entryPoint,    // 例: "main"
    const char* target)        // 例: "ps_5_0" / "vs_5_0"
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> blob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompileFromFile(
        filename,
        nullptr,                              // マクロ定義なし
        D3D_COMPILE_STANDARD_FILE_INCLUDE,    // ★重要：HLSL内の #include "Pbr.hlsli" を通すための設定
        entryPoint,
        target,
        flags,
        0,
        blob.GetAddressOf(),
        errorBlob.GetAddressOf());

    // ★重要：HLSLの文法エラーがあった場合、Visual Studioの「出力ウィンドウ」にエラー行数と理由を表示する
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));
    }

    return blob;
}

PbrShader::PbrShader(ID3D11Device* device)
{
    // =======================================================
    // 古い .cso ファイルを読み込むのをやめ、
    // 最新の HLSL ソースコードを起動時に直接コンパイルして読み込む！
    // =======================================================

    // ※注意：もしファイルが見つからずクラッシュする場合は、
    // パスを L"Shader/PbrVS.hlsl" から L"Data/Shader/PbrVS.hlsl" など環境に合わせて変更してください。

    // 1. 頂点シェーダーのコンパイルと生成
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob = CompileShaderFromFile(L"Shader/PbrVS.hlsl", "main", "vs_5_0");
    HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vertexShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    // 2. 入力レイアウトの生成
    hr = device->CreateInputLayout(Model::InputElementDescs.data(), static_cast<UINT>(Model::InputElementDescs.size()), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), inputLayout.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    // 3. ピクセルシェーダーのコンパイルと生成
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob = CompileShaderFromFile(L"Shader/PbrPS.hlsl", "main", "ps_5_0");
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, pixelShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    // 4. 定数バッファの生成
    GpuResourceUtils::CreateConstantBuffer(
        device,
        sizeof(CbMesh),
        meshConstantBuffer.GetAddressOf());
}

// 描画パス開始：シェーダーと定数バッファをバインド
void PbrShader::Begin(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc->PSSetConstantBuffers(0, 1, meshConstantBuffer.GetAddressOf());
}

// 描画パス更新：マテリアル情報をGPUに転送
void PbrShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
{
    ID3D11DeviceContext* dc = rc.deviceContext;
    const Model::Material* m = mesh.material;

    CbMesh cb{};
    cb.baseColorFactor = m->baseColor;
    cb.emissiveFactor = m->emissiveColor;
    cb.metalnessFactor = m->metalness;
    cb.roughnessFactor = m->roughness;
    cb.occlusionStrength = m->occlusionStrength;
    cb.alphaCutoff = m->alphaCutoff;
    cb.alphaMode = static_cast<int>(m->alphaMode);

    // テクスチャの有無をフラグで渡す（nullptr なら係数のみで計算）
    cb.hasBaseColorMap = m->baseMap ? 1 : 0;
    cb.hasNormalMap = m->normalMap ? 1 : 0;
    cb.hasMetalnessRoughnessMap = m->metalnessRoughnessMap ? 1 : 0;
    cb.hasEmissiveMap = m->emissiveMap ? 1 : 0;
    cb.hasOcclusionMap = m->occlusionMap ? 1 : 0;

    dc->UpdateSubresource(meshConstantBuffer.Get(), 0, 0, &cb, 0, 0);

    // テクスチャを t0〜t4 へ（PbrPS.hlsl の register と一致）
    ID3D11ShaderResourceView* srvs[] =
    {
        m->baseMap.Get(),               // t0: ベースカラー
        m->normalMap.Get(),             // t1: 法線マップ
        m->metalnessRoughnessMap.Get(), // t2: 金属度・粗さ
        m->emissiveMap.Get(),           // t3: 自己発光
        m->occlusionMap.Get(),          // t4: AO
    };
    dc->PSSetShaderResources(0, _countof(srvs), srvs);
}

// 描画パス終了
void PbrShader::End(const RenderContext& rc)
{
    // 必要に応じて後始末（現在は特になし）
}