#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

#include "Core/Misc.h"
#include "Graphics/GpuResourceUtils.h"
#include "Graphics/ShaderClass/PbrShader.h"

static Microsoft::WRL::ComPtr<ID3DBlob> CompileShaderFromFile(
    const wchar_t* filename, const char* entryPoint, const char* target)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    Microsoft::WRL::ComPtr<ID3DBlob> blob, errorBlob;
    HRESULT hr = D3DCompileFromFile(
        filename, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint, target, flags, 0,
        blob.GetAddressOf(), errorBlob.GetAddressOf());
    if (FAILED(hr))
    {
        if (errorBlob)
            OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
        _ASSERT_EXPR(SUCCEEDED(hr), L"PBR HLSL compile failed (see Output window)");
    }
    return blob;
}

PbrShader::PbrShader(ID3D11Device* device)
{
    auto vsBlob = CompileShaderFromFile(L"Shader/PbrVS.hlsl", "main", "vs_5_0");
    HRESULT hr = device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, vertexShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    hr = device->CreateInputLayout(
        Model::InputElementDescs.data(),
        static_cast<UINT>(Model::InputElementDescs.size()),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        inputLayout.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    auto psBlob = CompileShaderFromFile(L"Shader/PbrPS.hlsl", "main", "ps_5_0");
    hr = device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, pixelShader.GetAddressOf());
    _ASSERT_EXPR(SUCCEEDED(hr), HRTrace(hr));

    GpuResourceUtils::CreateConstantBuffer(
        device, sizeof(CbMesh), meshConstantBuffer.GetAddressOf());
}

void PbrShader::Begin(const RenderContext& rc)
{
    ID3D11DeviceContext* dc = rc.deviceContext;
    dc->IASetInputLayout(inputLayout.Get());
    dc->VSSetShader(vertexShader.Get(), nullptr, 0);
    dc->PSSetShader(pixelShader.Get(), nullptr, 0);
    ID3D11Buffer* cbs[] = { meshConstantBuffer.Get() };
    dc->PSSetConstantBuffers(0, _countof(cbs), cbs);
}

void PbrShader::Update(const RenderContext& rc, const Model::Mesh& mesh)
{
    ID3D11DeviceContext* dc = rc.deviceContext;
    const Model::Material* m = mesh.material;

    CbMesh cb{};
    cb.baseColorFactor          = m->baseColor;
    cb.emissiveFactor           = m->emissiveColor;
    cb.metalnessFactor          = m->metalness;
    cb.roughnessFactor          = m->roughness;
    cb.occlusionStrength        = m->occlusionStrength;
    cb.alphaCutoff              = m->alphaCutoff;
    cb.alphaMode                = static_cast<int>(m->alphaMode);
    cb.hasBaseColorMap          = m->baseMap               ? 1 : 0;
    cb.hasNormalMap             = m->normalMap             ? 1 : 0;
    cb.hasMetalnessRoughnessMap = m->metalnessRoughnessMap ? 1 : 0;
    cb.hasEmissiveMap           = m->emissiveMap           ? 1 : 0;
    cb.hasOcclusionMap          = m->occlusionMap          ? 1 : 0;
    dc->UpdateSubresource(meshConstantBuffer.Get(), 0, 0, &cb, 0, 0);

    ID3D11ShaderResourceView* srvs[] =
    {
        m->baseMap.Get(),
        m->normalMap.Get(),
        m->metalnessRoughnessMap.Get(),
        m->emissiveMap.Get(),
        m->occlusionMap.Get(),
    };
    dc->PSSetShaderResources(0, _countof(srvs), srvs);
}

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
