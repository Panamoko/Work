#include "Core/Misc.h"
#include "Graphics/GpuResourceUtils.h"
#include "Graphics/ShaderClass/PbrShader.h"

PbrShader::PbrShader(ID3D11Device* device)
{
    // =======================================================
    // ★完全リリース仕様：.hlslは不要。コンパイル済みの .cso だけを読み込む
    // =======================================================
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

void PbrShader::Begin(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc->PSSetConstantBuffers(0, 1, meshConstantBuffer.GetAddressOf());
}

void PbrShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
{
    ID3D11DeviceContext* dc = rc.deviceContext;
    const Model::Material* m = mesh.material;

    // ★安全装置：マテリアルが存在しない異常なメッシュはクラッシュを防ぐためスキップ
    if (m == nullptr) return;

    CbMesh cb{};
    cb.baseColorFactor = m->baseColor;
    cb.emissiveFactor = m->emissiveColor;
    cb.metalnessFactor = m->metalness;
    cb.roughnessFactor = m->roughness;
    cb.occlusionStrength = m->occlusionStrength;
    cb.alphaCutoff = m->alphaCutoff;
    cb.alphaMode = static_cast<int>(m->alphaMode);
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

void PbrShader::End(const RenderContext& rc)
{
    // 必要に応じて後始末（現在は特になし）
}