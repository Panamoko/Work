#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#include "Core/Misc.h"
#include "Graphics/GpuResourceUtils.h"
#include "Graphics/ShaderClass/ToonShader.h"

// Compile an HLSL file at runtime (so .hlsli edits always take effect; no cso).
static Microsoft::WRL::ComPtr<ID3DBlob> CompileToonShader(
    const wchar_t* filename, const char* entryPoint, const char* target)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    Microsoft::WRL::ComPtr<ID3DBlob> blob, errBlob;
    HRESULT hr = D3DCompileFromFile(
        filename, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint, target, flags, 0, blob.GetAddressOf(), errBlob.GetAddressOf());
    if (FAILED(hr))
    {
        if (errBlob) OutputDebugStringA(static_cast<const char*>(errBlob->GetBufferPointer()));
        _ASSERT_EXPR(SUCCEEDED(hr), L"Toon HLSL compile failed (see Output window)");
    }
    return blob;
}

// =======================================================
// ToonShader (runtime-compiled from Shader/ToonVS.hlsl + Shader/ToonPS.hlsl)
// =======================================================
ToonShader::ToonShader(ID3D11Device* device)
{
    auto vsBlob = CompileToonShader(L"Shader/ToonVS.hlsl", "main", "vs_5_0");
    HRESULT hr = device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vertexShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    hr = device->CreateInputLayout(
        Model::InputElementDescs.data(), static_cast<UINT>(Model::InputElementDescs.size()),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), inputLayout.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    auto psBlob = CompileToonShader(L"Shader/ToonPS.hlsl", "main", "ps_5_0");
    hr = device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, pixelShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    GpuResourceUtils::CreateConstantBuffer(device, sizeof(CbMesh), meshConstantBuffer.GetAddressOf());
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
// ToonOutlineShader (unchanged: still loads cso)
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