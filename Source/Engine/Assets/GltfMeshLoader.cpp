#include "Engine/Assets/GltfMeshLoader.h"
#include "Engine/Graphics/Mesh.h"

#include <tiny_gltf.h>
#include <vector>
#include <unordered_map>
#include <DirectXMath.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

namespace uel {
    namespace {

        struct LoadCtx {
            ID3D11Device* device;
            const tinygltf::Model& model;
            std::vector<MeshVertex>& vertices;
            std::vector<uint32_t>& indices;
            std::vector<Mesh::Submesh>& submeshes;
            std::unordered_map<int, ComPtr<ID3D11ShaderResourceView>>& texCache;
        };

        const unsigned char* AccessorPtr(const tinygltf::Model& m, const tinygltf::Accessor& acc, size_t& stride) {
            const tinygltf::BufferView& bv = m.bufferViews[acc.bufferView];
            const tinygltf::Buffer& buf = m.buffers[bv.buffer];
            int s = acc.ByteStride(bv);
            stride = (s > 0) ? (size_t)s : 0;
            return buf.data.data() + bv.byteOffset + acc.byteOffset;
        }

        XMFLOAT4 ReadColor(const unsigned char* data, size_t stride, size_t i, int compType, int numComp) {
            const unsigned char* e = data + stride * i;
            float c[4] = { 1,1,1,1 };
            for (int k = 0; k < numComp; ++k) switch (compType) {
            case TINYGLTF_COMPONENT_TYPE_FLOAT:          c[k] = reinterpret_cast<const float*>(e)[k]; break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  c[k] = reinterpret_cast<const uint8_t*>(e)[k] / 255.0f;   break;
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: c[k] = reinterpret_cast<const uint16_t*>(e)[k] / 65535.0f; break;
            default: break;
            }
            return { c[0], c[1], c[2], c[3] };
        }

        int BaseColorImageIndex(const tinygltf::Model& m, const tinygltf::Primitive& prim) {
            if (prim.material < 0 || prim.material >= (int)m.materials.size()) return -1;
            int texIdx = m.materials[prim.material].pbrMetallicRoughness.baseColorTexture.index;
            if (texIdx < 0 || texIdx >= (int)m.textures.size()) return -1;
            int imgIdx = m.textures[texIdx].source;
            if (imgIdx < 0 || imgIdx >= (int)m.images.size()) return -1;
            return imgIdx;
        }

        ComPtr<ID3D11ShaderResourceView> CreateSRVFromImage(ID3D11Device* device, const tinygltf::Image& img) {
            if (img.image.empty() || img.width <= 0 || img.height <= 0 || img.bits != 8) return nullptr;
            const unsigned char* pixels = img.image.data();
            std::vector<unsigned char> rgba;
            if (img.component == 3) {
                size_t n = (size_t)img.width * img.height;
                rgba.resize(n * 4);
                for (size_t i = 0; i < n; ++i) { rgba[i * 4] = img.image[i * 3]; rgba[i * 4 + 1] = img.image[i * 3 + 1]; rgba[i * 4 + 2] = img.image[i * 3 + 2]; rgba[i * 4 + 3] = 255; }
                pixels = rgba.data();
            }
            else if (img.component != 4) return nullptr;

            D3D11_TEXTURE2D_DESC td = {};
            td.Width = img.width; td.Height = img.height; td.MipLevels = 1; td.ArraySize = 1;
            td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
            td.Usage = D3D11_USAGE_IMMUTABLE; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA sd = {}; sd.pSysMem = pixels; sd.SysMemPitch = img.width * 4;
            ComPtr<ID3D11Texture2D> tex;
            if (FAILED(device->CreateTexture2D(&td, &sd, tex.GetAddressOf()))) return nullptr;
            ComPtr<ID3D11ShaderResourceView> srv;
            if (FAILED(device->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf()))) return nullptr;
            return srv;
        }

        XMMATRIX NodeLocalMatrix(const tinygltf::Node& n) {
            if (n.matrix.size() == 16) {
                XMFLOAT4X4 m;
                for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) m.m[i][j] = (float)n.matrix[i * 4 + j];
                return XMLoadFloat4x4(&m);
            }
            XMVECTOR S = (n.scale.size() == 3) ? XMVectorSet((float)n.scale[0], (float)n.scale[1], (float)n.scale[2], 0) : XMVectorSet(1, 1, 1, 0);
            XMVECTOR Q = (n.rotation.size() == 4) ? XMVectorSet((float)n.rotation[0], (float)n.rotation[1], (float)n.rotation[2], (float)n.rotation[3]) : XMVectorSet(0, 0, 0, 1);
            XMVECTOR T = (n.translation.size() == 3) ? XMVectorSet((float)n.translation[0], (float)n.translation[1], (float)n.translation[2], 0) : XMVectorSet(0, 0, 0, 0);
            return XMMatrixScalingFromVector(S) * XMMatrixRotationQuaternion(Q) * XMMatrixTranslationFromVector(T);
        }

        void AppendPrimitive(LoadCtx& c, const tinygltf::Primitive& prim, const XMMATRIX& world, const std::string& name)
        {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) return;
            auto itPos = prim.attributes.find("POSITION");
            if (itPos == prim.attributes.end()) return;
            const auto& model = c.model;

            const tinygltf::Accessor& posAcc = model.accessors[itPos->second];
            size_t posStride = 0; const unsigned char* posData = AccessorPtr(model, posAcc, posStride);
            if (posStride == 0) posStride = sizeof(float) * 3;

            const unsigned char* nrmData = nullptr; size_t nrmStride = 0;
            if (auto it = prim.attributes.find("NORMAL"); it != prim.attributes.end()) {
                const auto& a = model.accessors[it->second]; nrmData = AccessorPtr(model, a, nrmStride); if (nrmStride == 0) nrmStride = sizeof(float) * 3;
            }
            const unsigned char* uvData = nullptr; size_t uvStride = 0;
            if (auto it = prim.attributes.find("TEXCOORD_0"); it != prim.attributes.end()) {
                const auto& a = model.accessors[it->second]; uvData = AccessorPtr(model, a, uvStride); if (uvStride == 0) uvStride = sizeof(float) * 2;
            }
            const unsigned char* colData = nullptr; size_t colStride = 0; int colType = 0, colNum = 4;
            if (auto it = prim.attributes.find("COLOR_0"); it != prim.attributes.end()) {
                const auto& a = model.accessors[it->second]; colData = AccessorPtr(model, a, colStride); colType = a.componentType;
                colNum = (a.type == TINYGLTF_TYPE_VEC4) ? 4 : 3; if (colStride == 0) colStride = (colNum == 4 ? sizeof(float) * 4 : sizeof(float) * 3);
            }

            XMFLOAT4 matColor = { 1,1,1,1 };
            if (!colData && prim.material >= 0 && prim.material < (int)model.materials.size()) {
                const auto& bc = model.materials[prim.material].pbrMetallicRoughness.baseColorFactor;
                if (bc.size() == 4) matColor = { (float)bc[0],(float)bc[1],(float)bc[2],(float)bc[3] };
            }

            uint32_t baseVertex = (uint32_t)c.vertices.size();
            for (size_t i = 0; i < posAcc.count; ++i) {
                MeshVertex v{};
                const float* p = reinterpret_cast<const float*>(posData + posStride * i);
                XMStoreFloat3(&v.position, XMVector3TransformCoord(XMVectorSet(p[0], p[1], p[2], 1), world));
                if (nrmData) {
                    const float* n = reinterpret_cast<const float*>(nrmData + nrmStride * i);
                    XMStoreFloat3(&v.normal, XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(n[0], n[1], n[2], 0), world)));
                }
                else v.normal = { 0,1,0 };
                if (uvData) { const float* u = reinterpret_cast<const float*>(uvData + uvStride * i); v.uv = { u[0],u[1] }; }
                else v.uv = { 0,0 };
                v.color = colData ? ReadColor(colData, colStride, i, colType, colNum) : matColor;
                c.vertices.push_back(v);
            }

            uint32_t idxStart = (uint32_t)c.indices.size();
            if (prim.indices >= 0) {
                const tinygltf::Accessor& idxAcc = model.accessors[prim.indices];
                size_t idxStride = 0; const unsigned char* idxData = AccessorPtr(model, idxAcc, idxStride); if (idxStride == 0) idxStride = 2;
                for (size_t i = 0; i < idxAcc.count; ++i) {
                    const unsigned char* e = idxData + idxStride * i; uint32_t index = 0;
                    switch (idxAcc.componentType) {
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  index = *reinterpret_cast<const uint8_t*>(e);  break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: index = *reinterpret_cast<const uint16_t*>(e); break;
                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   index = *reinterpret_cast<const uint32_t*>(e); break;
                    default: break;
                    }
                    c.indices.push_back(baseVertex + index);
                }
            }
            else {
                for (size_t i = 0; i < posAcc.count; ++i) c.indices.push_back(baseVertex + (uint32_t)i);
            }

            Mesh::Submesh sm;
            sm.indexOffset = idxStart;
            sm.indexCount = (uint32_t)c.indices.size() - idxStart;
            sm.name = name;
            int img = BaseColorImageIndex(model, prim);
            if (img >= 0) {
                auto it = c.texCache.find(img);
                if (it != c.texCache.end()) sm.texture = it->second;
                else { sm.texture = CreateSRVFromImage(c.device, model.images[img]); c.texCache[img] = sm.texture; }
            }
            c.submeshes.push_back(sm);
        }

        void ProcessNode(LoadCtx& c, int nodeIdx, const XMMATRIX& parentWorld) {
            if (nodeIdx < 0 || nodeIdx >= (int)c.model.nodes.size()) return;
            const tinygltf::Node& node = c.model.nodes[nodeIdx];
            XMMATRIX world = NodeLocalMatrix(node) * parentWorld;
            if (node.mesh >= 0 && node.mesh < (int)c.model.meshes.size()) {
                const auto& mesh = c.model.meshes[node.mesh];
                std::string nm = !node.name.empty() ? node.name : mesh.name;
                for (const auto& prim : mesh.primitives) AppendPrimitive(c, prim, world, nm);
            }
            for (int child : node.children) ProcessNode(c, child, world);
        }
    }

    std::shared_ptr<Mesh> LoadGltfMesh(ID3D11Device* device, const std::string& path)
    {
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string err, warn;
        if (!loader.LoadBinaryFromFile(&model, &err, &warn, path)) return nullptr;

        std::vector<MeshVertex> vertices;
        std::vector<uint32_t>   indices;
        std::vector<Mesh::Submesh> submeshes;
        std::unordered_map<int, ComPtr<ID3D11ShaderResourceView>> texCache;
        LoadCtx ctx{ device, model, vertices, indices, submeshes, texCache };

        if (!model.scenes.empty()) {
            int si = (model.defaultScene >= 0) ? model.defaultScene : 0;
            for (int root : model.scenes[si].nodes) ProcessNode(ctx, root, XMMatrixIdentity());
        }
        else {
            for (const auto& mesh : model.meshes)
                for (const auto& prim : mesh.primitives)
                    AppendPrimitive(ctx, prim, XMMatrixIdentity(), mesh.name);
        }

        if (vertices.empty() || indices.empty()) return nullptr;

        auto out = std::make_shared<Mesh>();
        if (!out->Create(device, vertices.data(), vertices.size(), indices.data(), indices.size(), submeshes))
            return nullptr;
        return out;
    }

} // namespace uel