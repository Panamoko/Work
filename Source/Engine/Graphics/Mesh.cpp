#include "Engine/Graphics/Mesh.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace uel {

    bool Mesh::Create(ID3D11Device* device,
        const MeshVertex* vertices, size_t vertexCount,
        const uint32_t* indices, size_t idxCount,
        const std::vector<Submesh>& subs)
    {
        if (!vertices || vertexCount == 0 || !indices || idxCount == 0) return false;
        vb.Reset(); ib.Reset();

        D3D11_BUFFER_DESC bd = {}; D3D11_SUBRESOURCE_DATA sd = {};
        bd.Usage = D3D11_USAGE_IMMUTABLE;
        bd.ByteWidth = (UINT)(sizeof(MeshVertex) * vertexCount);
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        sd.pSysMem = vertices;
        if (FAILED(device->CreateBuffer(&bd, &sd, vb.GetAddressOf()))) return false;

        bd.ByteWidth = (UINT)(sizeof(uint32_t) * idxCount);
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        sd.pSysMem = indices;
        if (FAILED(device->CreateBuffer(&bd, &sd, ib.GetAddressOf()))) return false;

        submeshes = subs;

        cpuPositions.resize(vertexCount);
        for (size_t i = 0; i < vertexCount; ++i) cpuPositions[i] = vertices[i].position;
        cpuIndices.assign(indices, indices + idxCount);

        return true;
    }

    void Mesh::Bind(ID3D11DeviceContext* context) const
    {
        UINT stride = sizeof(MeshVertex), offset = 0;
        context->IASetVertexBuffers(0, 1, vb.GetAddressOf(), &stride, &offset);
        context->IASetIndexBuffer(ib.Get(), DXGI_FORMAT_R32_UINT, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    std::shared_ptr<Mesh> Mesh::CreateCube(ID3D11Device* device)
    {
        MeshVertex v[] = {
            {{ 0.5f,-0.5f,-0.5f},{ 1,0,0}},{{ 0.5f, 0.5f,-0.5f},{ 1,0,0}},{{ 0.5f, 0.5f, 0.5f},{ 1,0,0}},{{ 0.5f,-0.5f, 0.5f},{ 1,0,0}},
            {{-0.5f,-0.5f, 0.5f},{-1,0,0}},{{-0.5f, 0.5f, 0.5f},{-1,0,0}},{{-0.5f, 0.5f,-0.5f},{-1,0,0}},{{-0.5f,-0.5f,-0.5f},{-1,0,0}},
            {{-0.5f, 0.5f,-0.5f},{0, 1,0}},{{-0.5f, 0.5f, 0.5f},{0, 1,0}},{{ 0.5f, 0.5f, 0.5f},{0, 1,0}},{{ 0.5f, 0.5f,-0.5f},{0, 1,0}},
            {{-0.5f,-0.5f, 0.5f},{0,-1,0}},{{-0.5f,-0.5f,-0.5f},{0,-1,0}},{{ 0.5f,-0.5f,-0.5f},{0,-1,0}},{{ 0.5f,-0.5f, 0.5f},{0,-1,0}},
            {{ 0.5f,-0.5f, 0.5f},{0,0, 1}},{{ 0.5f, 0.5f, 0.5f},{0,0, 1}},{{-0.5f, 0.5f, 0.5f},{0,0, 1}},{{-0.5f,-0.5f, 0.5f},{0,0, 1}},
            {{-0.5f,-0.5f,-0.5f},{0,0,-1}},{{-0.5f, 0.5f,-0.5f},{0,0,-1}},{{ 0.5f, 0.5f,-0.5f},{0,0,-1}},{{ 0.5f,-0.5f,-0.5f},{0,0,-1}},
        };
        for (auto& vert : v) { vert.color = { 1,1,1,1 }; vert.uv = { 0.0f, 0.0f }; }

        uint32_t idx[36];
        for (int f = 0; f < 6; ++f) {
            int b = f * 4, o = f * 6;
            idx[o + 0] = b + 0; idx[o + 1] = b + 1; idx[o + 2] = b + 2;
            idx[o + 3] = b + 0; idx[o + 4] = b + 2; idx[o + 5] = b + 3;
        }

        std::vector<Submesh> subs;
        Submesh sm; sm.indexOffset = 0; sm.indexCount = 36; sm.name = "cube";
        subs.push_back(sm);

        auto m = std::make_shared<Mesh>();
        if (!m->Create(device, v, 24, idx, 36, subs)) return nullptr;
        return m;
    }



} // namespace uel