#include "Core/Misc.h"
#include "Graphics/GpuResourceUtils.h"
#include "Graphics/ShaderClass/ToonShader.h"

// =======================================================
// ToonShader 実装
// =======================================================
ToonShader::ToonShader(ID3D11Device* device)
{
    // ★ VSでコンパイルされた最新の .cso を確実に読み込む
    GpuResourceUtils::LoadVertexShader(
        device,
        "Data/CompiledShader/ToonVS.cso",
        Model::InputElementDescs.data(),
        static_cast<UINT>(Model::InputElementDescs.size()),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

    GpuResourceUtils::LoadPixelShader(
        device,
        "Data/CompiledShader/ToonPS.cso",
        pixelShader.GetAddressOf());

    GpuResourceUtils::CreateConstantBuffer(
        device,
        sizeof(CbMesh),
        meshConstantBuffer.GetAddressOf());
}

void ToonShader::Begin(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;
    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc->PSSetConstantBuffers(0, 1, meshConstantBuffer.GetAddressOf());
}

void ToonShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
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

    ID3D11ShaderResourceView* srvs[] = {
        m->baseMap.Get(), m->normalMap.Get(), m->metalnessRoughnessMap.Get(), m->emissiveMap.Get(), m->occlusionMap.Get(),
    };
    dc->PSSetShaderResources(0, _countof(srvs), srvs);
}

void ToonShader::End(const RenderContext& rc) {}

// =======================================================
// ToonOutlineShader 実装
// =======================================================
ToonOutlineShader::ToonOutlineShader(ID3D11Device* device)
{
    GpuResourceUtils::LoadVertexShader(
        device,
        "Data/CompiledShader/ToonOutlineVS.cso",
        Model::InputElementDescs.data(),
        static_cast<UINT>(Model::InputElementDescs.size()),
        inputLayout.GetAddressOf(),
        vertexShader.GetAddressOf());

    GpuResourceUtils::LoadPixelShader(
        device,
        "Data/CompiledShader/ToonOutlinePS.cso",
        pixelShader.GetAddressOf());

    GpuResourceUtils::CreateConstantBuffer(
        device,
        sizeof(CbOutline),
        outlineConstantBuffer.GetAddressOf());
}

void ToonOutlineShader::Begin(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;
    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);
    dc->VSSetConstantBuffers(0, 1, outlineConstantBuffer.GetAddressOf());
}

void ToonOutlineShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
{
    ID3D11DeviceContext* dc = rc.deviceContext;

    CbOutline cb{};
    cb.outlineWidth = 0.005f;
    dc->UpdateSubresource(outlineConstantBuffer.Get(), 0, 0, &cb, 0, 0);
}

void ToonOutlineShader::End(const RenderContext& rc) {}