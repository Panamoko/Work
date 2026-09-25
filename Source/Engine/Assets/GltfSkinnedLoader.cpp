#include "Engine/Assets/GltfSkinnedLoader.h"
#include <tiny_gltf.h>
#include <DirectXMath.h>
#include <unordered_map>
#include <cstring>
#include <string>

using namespace DirectX;

namespace uel {
    namespace {

        const unsigned char* AccPtr(const tinygltf::Model& m, const tinygltf::Accessor& a, size_t& stride) {
            const auto& bv = m.bufferViews[a.bufferView];
            const auto& buf = m.buffers[bv.buffer];
            int s = a.ByteStride(bv);
            stride = (s > 0) ? (size_t)s : 0;
            return buf.data.data() + bv.byteOffset + a.byteOffset;
        }

        float ReadCompF(const unsigned char* e, int compType, int k, bool normalized) {
            switch (compType) {
            case TINYGLTF_COMPONENT_TYPE_FLOAT:          return reinterpret_cast<const float*>(e)[k];
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: { float v = reinterpret_cast<const uint8_t*>(e)[k];  return normalized ? v / 255.0f : v; }
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: { float v = reinterpret_cast<const uint16_t*>(e)[k]; return normalized ? v / 65535.0f : v; }
            default: return 0.0f;
            }
        }

        uint32_t ReadIndexU32(const unsigned char* e, int compType, int k) {
            switch (compType) {
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  return reinterpret_cast<const uint8_t*>(e)[k];
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: return reinterpret_cast<const uint16_t*>(e)[k];
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:   return reinterpret_cast<const uint32_t*>(e)[k];
            default: return 0;
            }
        }

        void NodeTRS(const tinygltf::Node& n, XMFLOAT3& t, XMFLOAT4& r, XMFLOAT3& s) {
            t = { 0,0,0 }; r = { 0,0,0,1 }; s = { 1,1,1 };
            if (n.matrix.size() == 16) {
                XMFLOAT4X4 m{};
                for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) m.m[i][j] = (float)n.matrix[i * 4 + j]; // 列優先→行ベクトル形
                XMVECTOR S, Q, T;
                if (XMMatrixDecompose(&S, &Q, &T, XMLoadFloat4x4(&m))) {
                    XMStoreFloat3(&s, S); XMStoreFloat4(&r, Q); XMStoreFloat3(&t, T);
                }
                return;
            }
            if (n.translation.size() == 3) t = { (float)n.translation[0],(float)n.translation[1],(float)n.translation[2] };
            if (n.rotation.size() == 4)    r = { (float)n.rotation[0],(float)n.rotation[1],(float)n.rotation[2],(float)n.rotation[3] };
            if (n.scale.size() == 3)       s = { (float)n.scale[0],(float)n.scale[1],(float)n.scale[2] };
        }

    } // namespace

    bool LoadSkinnedModel(const std::string& path, SkinnedModel& out)
    {
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string err, warn;
        if (!loader.LoadBinaryFromFile(&model, &err, &warn, path)) return false;

        // skin+mesh を持つノードを探す
        int skinnedNode = -1;
        for (int i = 0; i < (int)model.nodes.size(); ++i)
            if (model.nodes[i].skin >= 0 && model.nodes[i].mesh >= 0) { skinnedNode = i; break; }
        if (skinnedNode < 0) return false;

        const tinygltf::Node& node = model.nodes[skinnedNode];
        const tinygltf::Skin& skin = model.skins[node.skin];

        // 親マップ（node->node）
        std::vector<int> nodeParent(model.nodes.size(), -1);
        for (int i = 0; i < (int)model.nodes.size(); ++i)
            for (int c : model.nodes[i].children) nodeParent[c] = i;

        // joint node -> bone index
        std::unordered_map<int, int> nodeToBone;
        for (int b = 0; b < (int)skin.joints.size(); ++b) nodeToBone[skin.joints[b]] = b;

        // inverseBind accessor
        const unsigned char* ibmData = nullptr; size_t ibmStride = 0;
        if (skin.inverseBindMatrices >= 0) {
            const auto& a = model.accessors[skin.inverseBindMatrices];
            ibmData = AccPtr(model, a, ibmStride);
            if (ibmStride == 0) ibmStride = sizeof(float) * 16;
        }

        // bones
        out.skeleton.bones.clear();
        out.skeleton.bones.resize(skin.joints.size());
        for (int b = 0; b < (int)skin.joints.size(); ++b) {
            int jn = skin.joints[b];
            SkeletonBone& bone = out.skeleton.bones[b];
            bone.name = model.nodes[jn].name;
            NodeTRS(model.nodes[jn], bone.t, bone.r, bone.s);
            auto it = nodeToBone.find(nodeParent[jn]);
            bone.parent = (it != nodeToBone.end()) ? it->second : -1;
            if (ibmData) {
                const float* m = reinterpret_cast<const float*>(ibmData + ibmStride * b);
                for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) bone.inverseBind.m[i][j] = m[i * 4 + j]; // 列優先→行ベクトル形
            }
            else XMStoreFloat4x4(&bone.inverseBind, XMMatrixIdentity());
        }

        // 頂点/インデックス（メッシュローカル空間。glTF仕様によりskinned meshのnode変換は無視）
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];
        for (const auto& prim : mesh.primitives) {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;
            auto itPos = prim.attributes.find("POSITION");
            if (itPos == prim.attributes.end()) continue;

            const auto& posAcc = model.accessors[itPos->second];
            size_t posStride = 0; const unsigned char* posData = AccPtr(model, posAcc, posStride);
            if (posStride == 0) posStride = sizeof(float) * 3;

            const unsigned char* nrmData = nullptr; size_t nrmStride = 0;
            if (auto it = prim.attributes.find("NORMAL"); it != prim.attributes.end()) { const auto& a = model.accessors[it->second]; nrmData = AccPtr(model, a, nrmStride); if (!nrmStride) nrmStride = sizeof(float) * 3; }
            const unsigned char* uvData = nullptr; size_t uvStride = 0;
            if (auto it = prim.attributes.find("TEXCOORD_0"); it != prim.attributes.end()) { const auto& a = model.accessors[it->second]; uvData = AccPtr(model, a, uvStride); if (!uvStride) uvStride = sizeof(float) * 2; }
            const unsigned char* jData = nullptr; size_t jStride = 0; int jType = 0;
            if (auto it = prim.attributes.find("JOINTS_0"); it != prim.attributes.end()) { const auto& a = model.accessors[it->second]; jData = AccPtr(model, a, jStride); jType = a.componentType; if (!jStride) jStride = (jType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ? 4 : 8); }
            const unsigned char* wData = nullptr; size_t wStride = 0; int wType = 0; bool wNorm = false;
            if (auto it = prim.attributes.find("WEIGHTS_0"); it != prim.attributes.end()) { const auto& a = model.accessors[it->second]; wData = AccPtr(model, a, wStride); wType = a.componentType; wNorm = a.normalized; if (!wStride) wStride = sizeof(float) * 4; }

            uint32_t baseVertex = (uint32_t)out.vertices.size();
            for (size_t i = 0; i < posAcc.count; ++i) {
                SkinnedVertex v{};
                const float* p = reinterpret_cast<const float*>(posData + posStride * i);
                v.position = { p[0],p[1],p[2] };
                if (nrmData) { const float* n = reinterpret_cast<const float*>(nrmData + nrmStride * i); v.normal = { n[0],n[1],n[2] }; }
                else v.normal = { 0,1,0 };
                if (uvData) { const float* u = reinterpret_cast<const float*>(uvData + uvStride * i); v.uv = { u[0],u[1] }; }
                else v.uv = { 0,0 };
                v.color = { 1,1,1,1 };
                v.bones[0] = v.bones[1] = v.bones[2] = v.bones[3] = 0;
                v.weights[0] = 1; v.weights[1] = v.weights[2] = v.weights[3] = 0;
                if (jData) { const unsigned char* e = jData + jStride * i; for (int k = 0; k < 4; ++k) v.bones[k] = ReadIndexU32(e, jType, k); }
                if (wData) { const unsigned char* e = wData + wStride * i; float sum = 0; for (int k = 0; k < 4; ++k) { v.weights[k] = ReadCompF(e, wType, k, wNorm); sum += v.weights[k]; } if (sum > 1e-6f) for (int k = 0; k < 4; ++k) v.weights[k] /= sum; }
                out.vertices.push_back(v);
            }

            uint32_t idxStart = (uint32_t)out.indices.size();
            if (prim.indices >= 0) {
                const auto& ia = model.accessors[prim.indices];
                size_t is = 0; const unsigned char* id = AccPtr(model, ia, is); if (!is) is = 2;
                for (size_t i = 0; i < ia.count; ++i) { const unsigned char* e = id + is * i; out.indices.push_back(baseVertex + ReadIndexU32(e, ia.componentType, 0)); }
            }
            else {
                for (size_t i = 0; i < posAcc.count; ++i) out.indices.push_back(baseVertex + (uint32_t)i);
            }

            SkinnedSubmesh sm;
            sm.indexOffset = idxStart;
            sm.indexCount = (uint32_t)out.indices.size() - idxStart;
            sm.name = mesh.name;
            sm.texture = -1;
            if (prim.material >= 0 && prim.material < (int)model.materials.size()) {
                int ti = model.materials[prim.material].pbrMetallicRoughness.baseColorTexture.index;
                if (ti >= 0 && ti < (int)model.textures.size()) {
                    int img = model.textures[ti].source;
                    if (img >= 0 && img < (int)model.images.size()) {
                        const auto& im = model.images[img];
                        if (!im.image.empty() && im.width > 0 && im.height > 0 && im.bits == 8) {
                            SkinnedTexture t; t.width = im.width; t.height = im.height; t.rgba.resize((size_t)im.width * im.height * 4);
                            if (im.component == 4) std::memcpy(t.rgba.data(), im.image.data(), t.rgba.size());
                            else if (im.component == 3) { size_t n = (size_t)im.width * im.height; for (size_t k = 0; k < n; ++k) { t.rgba[k * 4] = im.image[k * 3]; t.rgba[k * 4 + 1] = im.image[k * 3 + 1]; t.rgba[k * 4 + 2] = im.image[k * 3 + 2]; t.rgba[k * 4 + 3] = 255; } }
                            else t.rgba.clear();
                            if (!t.rgba.empty()) { sm.texture = (int)out.textures.size(); out.textures.push_back(std::move(t)); }
                        }
                    }
                }
            }
            out.submeshes.push_back(sm);
        }

        // animations
        for (const auto& anim : model.animations) {
            AnimationClip clip;
            clip.name = anim.name.empty() ? ("clip" + std::to_string(out.clips.size())) : anim.name;
            std::unordered_map<int, int> boneToTrack;
            float maxT = 0.0f;
            for (const auto& ch : anim.channels) {
                auto itB = nodeToBone.find(ch.target_node);
                if (itB == nodeToBone.end()) continue;
                int bone = itB->second;
                const auto& samp = anim.samplers[ch.sampler];
                const auto& inAcc = model.accessors[samp.input];
                const auto& outAcc = model.accessors[samp.output];
                size_t inStride = 0;  const unsigned char* inData = AccPtr(model, inAcc, inStride);  if (!inStride) inStride = sizeof(float);
                size_t outStride = 0; const unsigned char* outData = AccPtr(model, outAcc, outStride);
                size_t elemSize = (ch.target_path == "rotation") ? sizeof(float) * 4 : sizeof(float) * 3;
                if (outStride == 0) outStride = elemSize;

                int ti;
                auto itT = boneToTrack.find(bone);
                if (itT == boneToTrack.end()) { ti = (int)clip.tracks.size(); clip.tracks.push_back({}); clip.tracks[ti].bone = bone; boneToTrack[bone] = ti; }
                else ti = itT->second;
                BoneTrack& tr = clip.tracks[ti];

                for (size_t k = 0; k < inAcc.count; ++k) {
                    float t = *reinterpret_cast<const float*>(inData + inStride * k);
                    if (t > maxT) maxT = t;
                    const float* v = reinterpret_cast<const float*>(outData + outStride * k);
                    if (ch.target_path == "translation") tr.pos.push_back({ t,{ v[0],v[1],v[2] } });
                    else if (ch.target_path == "scale")   tr.scl.push_back({ t,{ v[0],v[1],v[2] } });
                    else if (ch.target_path == "rotation")tr.rot.push_back({ t,{ v[0],v[1],v[2],v[3] } });
                }
            }
            clip.duration = maxT;
            out.clips.push_back(std::move(clip));
        }

        return out.Valid();
    }

} // namespace uel