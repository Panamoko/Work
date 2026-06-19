#include "Core/Misc.h"
#include "Graphics/GpuResourceUtils.h"
#include "Graphics/ShaderClass/PbrShader.h"

PbrShader::PbrShader(ID3D11Device* device)
{
    // 頂点シェーダー＋入力レイアウト（Model::InputElementDescs を流用）
    // cso の読込先は既存シェーダーと同じ "Data/CompiledShader/" に統一する。
    // vcxproj 側で ObjectFileOutput を Data\CompiledShader\ に設定しておくこと。
    GpuResourceUtils::LoadVertexShader(
        device,
        "Data/CompiledShader/PbrVS.cso",
        Model::InputElementDescs.data(),
        static_cast<UINT>(Model::InputElementDescs.size()),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

    GpuResourceUtils::LoadPixelShader(
        device,
        "Data/CompiledShader/PbrPS.cso",
        pixelShader.GetAddressOf());

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

    // マテリアル定数バッファを b0 にセット
    // CbScene(b7) / CbSkeleton(b6) は ModelRenderer::Render が共通でバインド済みなので不要
    ID3D11Buffer* cbs[] = { meshConstantBuffer.Get() };
    dc->PSSetConstantBuffers(0, _countof(cbs), cbs);
}

// メッシュ毎の更新：マテリアル値とテクスチャを GPU に転送
void PbrShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
{
    ID3D11DeviceContext* dc = rc.deviceContext;
    const Model::Material* m = mesh.material;

    // マテリアル係数を定数バッファへ
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
        m->baseMap.Get(),                // t0: ベースカラー
        m->normalMap.Get(),              // t1: 法線マップ
        m->metalnessRoughnessMap.Get(),  // t2: 金属度・粗さ
        m->emissiveMap.Get(),            // t3: 自己発光
        m->occlusionMap.Get(),           // t4: AO
    };
    dc->PSSetShaderResources(0, _countof(srvs), srvs);
}

// 描画パス終了：次のシェーダーに影響しないよう全バインドを解除
void PbrShader::End(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    dc->VSSetShader(nullptr, nullptr, 0);
    dc->PSSetShader(nullptr, nullptr, 0);
    dc->IASetInputLayout(nullptr);

    ID3D11Buffer* cbs[] = { nullptr };
    dc->PSSetConstantBuffers(0, _countof(cbs), cbs);

    ID3D11ShaderResourceView* srvs[] = { nullptr, nullptr, nullptr, nullptr, nullptr };
    dc->PSSetShaderResources(0, _countof(srvs), srvs);
}