#include "Engine/Assets/GltfMeshLoader.h"
#include "Engine/Graphics/Mesh.h"

#include <tiny_gltf.h>   // 実装は GLTFImporter.cpp。ここは宣言のみ（マクロ定義しない）。
#include <vector>

namespace uel {
    namespace {

        // 画像はメッシュ用途では不要。デコードを避けるための no-op。
        bool NoOpLoadImage(tinygltf::Image*, const int, std::string*, std::string*,
            int, int, const unsigned char*, int, void*) {
            return true;
        }

        const unsigned char* AccessorPtr(const tinygltf::Model& m, const tinygltf::Accessor& acc, size_t& stride)
        {
            const tinygltf::BufferView& bv = m.bufferViews[acc.bufferView];
            const tinygltf::Buffer& buf = m.buffers[bv.buffer];
            int s = acc.ByteStride(bv);
            stride = (s > 0) ? (size_t)s : 0;
            return buf.data.data() + bv.byteOffset + acc.byteOffset;
        }
    }

    std::shared_ptr<Mesh> LoadGltfMesh(ID3D11Device* device, const std::string& path)
    {
        tinygltf::TinyGLTF loader;
        loader.SetImageLoader(NoOpLoadImage, nullptr);

        tinygltf::Model model;
        std::string err, warn;
        if (!loader.LoadBinaryFromFile(&model, &err, &warn, path))
            return nullptr;

        std::vector<MeshVertex> vertices;
        std::vector<uint32_t>   indices;

        for (const auto& mesh : model.meshes)
        {
            for (const auto& prim : mesh.primitives)
            {
                if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;
                auto itPos = prim.attributes.find("POSITION");
                if (itPos == prim.attributes.end()) continue;

                const tinygltf::Accessor& posAcc = model.accessors[itPos->second];
                size_t posStride = 0;
                const unsigned char* posData = AccessorPtr(model, posAcc, posStride);
                if (posStride == 0) posStride = sizeof(float) * 3;

                const unsigned char* nrmData = nullptr; size_t nrmStride = 0;
                auto itNrm = prim.attributes.find("NORMAL");
                if (itNrm != prim.attributes.end()) {
                    const tinygltf::Accessor& nrmAcc = model.accessors[itNrm->second];
                    nrmData = AccessorPtr(model, nrmAcc, nrmStride);
                    if (nrmStride == 0) nrmStride = sizeof(float) * 3;
                }

                uint32_t baseVertex = (uint32_t)vertices.size();
                for (size_t i = 0; i < posAcc.count; ++i) {
                    MeshVertex v{};
                    const float* p = reinterpret_cast<const float*>(posData + posStride * i);
                    v.position = { p[0], p[1], p[2] };
                    if (nrmData) {
                        const float* n = reinterpret_cast<const float*>(nrmData + nrmStride * i);
                        v.normal = { n[0], n[1], n[2] };
                    }
                    else {
                        v.normal = { 0.0f, 1.0f, 0.0f }; // 法線が無ければ仮の上向き
                    }
                    vertices.push_back(v);
                }

                if (prim.indices >= 0) {
                    const tinygltf::Accessor& idxAcc = model.accessors[prim.indices];
                    size_t idxStride = 0;
                    const unsigned char* idxData = AccessorPtr(model, idxAcc, idxStride);
                    if (idxStride == 0) idxStride = 2; // 保険
                    for (size_t i = 0; i < idxAcc.count; ++i) {
                        const unsigned char* e = idxData + idxStride * i;
                        uint32_t index = 0;
                        switch (idxAcc.componentType) {
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  index = *reinterpret_cast<const uint8_t*>(e);  break;
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: index = *reinterpret_cast<const uint16_t*>(e); break;
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   index = *reinterpret_cast<const uint32_t*>(e); break;
                        default: break;
                        }
                        indices.push_back(baseVertex + index);
                    }
                }
                else {
                    for (size_t i = 0; i < posAcc.count; ++i)
                        indices.push_back(baseVertex + (uint32_t)i);
                }
            }
        }

        if (vertices.empty() || indices.empty()) return nullptr;

        auto out = std::make_shared<Mesh>();
        if (!out->Create(device, vertices.data(), vertices.size(), indices.data(), indices.size()))
            return nullptr;
        return out;
    }

} // namespace uel