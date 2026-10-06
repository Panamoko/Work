#include "Engine/Assets/GltfSkinnedLoader.h"
#include <tiny_gltf.h>
#include <DirectXMath.h>
#include <unordered_map>
#include <functional>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

using namespace DirectX;

namespace uel {
    namespace {

        // 画像が壊れていても、モデルの読み込みは続ける（その画像はテクスチャ無しとして扱う）。
        // tinygltf の既定では、画像を1枚でも展開できないとファイル全体が読み込み失敗になる。
        bool LoadImageLenient(tinygltf::Image* image, const int index, std::string*, std::string* warn,
            int reqWidth, int reqHeight, const unsigned char* bytes, int size, void*)
        {
            std::string e;
            if (!tinygltf::LoadImageData(image, index, &e, warn, reqWidth, reqHeight, bytes, size, nullptr)) {
                image->image.clear(); image->width = 0; image->height = 0;
            }
            return true;
        }

        // アニメだけを読むときは、画像を展開しない（時間もメモリも使わない）
        bool SkipImage(tinygltf::Image*, const int, std::string*, std::string*,
            int, int, const unsigned char*, int, void*)
        {
            return true;
        }

        // アクセサの先頭を返す。データを持たないアクセサ（sparse だけ、など）は nullptr。
        const unsigned char* AccPtr(const tinygltf::Model& m, const tinygltf::Accessor& a, size_t& stride) {
            stride = 0;
            if (a.bufferView < 0 || a.bufferView >= (int)m.bufferViews.size()) return nullptr;
            const auto& bv = m.bufferViews[a.bufferView];
            if (bv.buffer < 0 || bv.buffer >= (int)m.buffers.size()) return nullptr;
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
                for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) m.m[i][j] = (float)n.matrix[i * 4 + j];
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

        // 同じ画像を複数のサブメッシュが使うモデルが多いので、画像ごとに1回だけ取り込む。
        int LoadTexture(const tinygltf::Model& model, int material, SkinnedModel& out, std::unordered_map<int, int>& imageToTexture) {
            if (material < 0 || material >= (int)model.materials.size()) return -1;
            int ti = model.materials[material].pbrMetallicRoughness.baseColorTexture.index;
            if (ti < 0 || ti >= (int)model.textures.size()) return -1;
            int img = model.textures[ti].source;
            if (img < 0 || img >= (int)model.images.size()) return -1;
            auto itC = imageToTexture.find(img);
            if (itC != imageToTexture.end()) return itC->second;
            const auto& im = model.images[img];
            if (im.image.empty() || im.width <= 0 || im.height <= 0 || im.bits != 8) return -1;
            SkinnedTexture t; t.width = im.width; t.height = im.height; t.rgba.resize((size_t)im.width * im.height * 4);
            if (im.component == 4) std::memcpy(t.rgba.data(), im.image.data(), t.rgba.size());
            else if (im.component == 3) { size_t n = (size_t)im.width * im.height; for (size_t k = 0; k < n; ++k) { t.rgba[k * 4] = im.image[k * 3]; t.rgba[k * 4 + 1] = im.image[k * 3 + 1]; t.rgba[k * 4 + 2] = im.image[k * 3 + 2]; t.rgba[k * 4 + 3] = 255; } }
            else return -1;
            int idx = (int)out.textures.size(); out.textures.push_back(std::move(t));
            imageToTexture[img] = idx;
            return idx;
        }

        bool SameMatrix(const XMFLOAT4X4& a, const XMFLOAT4X4& b) {
            for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j)
                if (std::fabs(a.m[i][j] - b.m[i][j]) > 1e-4f) return false;
            return true;
        }

        // glTF のアニメ1つを AnimationClip にする。
        // resolveBone は「チャンネルが動かすノード番号 -> skeleton.bones の index（無ければ -1）」。
        AnimationClip ReadClip(const tinygltf::Model& model, const tinygltf::Animation& anim,
            const std::function<int(int)>& resolveBone)
        {
            AnimationClip clip;
            clip.name = anim.name;
            std::unordered_map<int, int> boneToTrack;
            float maxT = 0.0f;
            for (const auto& ch : anim.channels) {
                const bool isT = (ch.target_path == "translation");
                const bool isR = (ch.target_path == "rotation");
                const bool isS = (ch.target_path == "scale");
                if (!isT && !isR && !isS) continue;                      // weights（モーフ）は未対応
                if (ch.sampler < 0 || ch.sampler >= (int)anim.samplers.size()) continue;
                const int bone = resolveBone(ch.target_node);
                if (bone < 0) continue;

                const auto& samp = anim.samplers[ch.sampler];
                if (samp.input < 0 || samp.input >= (int)model.accessors.size()) continue;
                if (samp.output < 0 || samp.output >= (int)model.accessors.size()) continue;
                const auto& inAcc = model.accessors[samp.input];
                const auto& outAcc = model.accessors[samp.output];
                if (inAcc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) continue;
                if (outAcc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) continue;   // 量子化された出力は未対応
                size_t inStride = 0;  const unsigned char* inData = AccPtr(model, inAcc, inStride);
                size_t outStride = 0; const unsigned char* outData = AccPtr(model, outAcc, outStride);
                if (!inData || !outData) continue;
                if (!inStride) inStride = sizeof(float);
                const size_t elemSize = isR ? sizeof(float) * 4 : sizeof(float) * 3;
                if (!outStride) outStride = elemSize;

                // CUBICSPLINE は1キーにつき（入り接線・値・出接線）の3つが並ぶ。値だけを使い、間は線形でつなぐ。
                const bool cubic = (samp.interpolation == "CUBICSPLINE");
                const size_t perKey = cubic ? 3 : 1;
                const size_t valueAt = cubic ? 1 : 0;
                size_t count = inAcc.count;
                if (outAcc.count / perKey < count) count = outAcc.count / perKey;

                int ti;
                auto itT = boneToTrack.find(bone);
                if (itT == boneToTrack.end()) { ti = (int)clip.tracks.size(); clip.tracks.push_back({}); clip.tracks[ti].bone = bone; boneToTrack[bone] = ti; }
                else ti = itT->second;
                BoneTrack& tr = clip.tracks[ti];

                for (size_t k = 0; k < count; ++k) {
                    float t = *reinterpret_cast<const float*>(inData + inStride * k);
                    if (t > maxT) maxT = t;
                    const float* v = reinterpret_cast<const float*>(outData + outStride * (k * perKey + valueAt));
                    if (isT)      tr.pos.push_back({ t,{ v[0],v[1],v[2] } });
                    else if (isS) tr.scl.push_back({ t,{ v[0],v[1],v[2] } });
                    else          tr.rot.push_back({ t,{ v[0],v[1],v[2],v[3] } });
                }
            }
            clip.duration = maxT;
            return clip;
        }

    } // namespace

    bool LoadSkinnedModel(const std::string& path, SkinnedModel& out)
    {
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string err, warn;
        loader.SetImageLoader(LoadImageLenient, nullptr);
        if (!loader.LoadBinaryFromFile(&model, &err, &warn, path)) return false;

        const int nodeCount = (int)model.nodes.size();

        // そのノードのメッシュがスキン付きかどうか
        auto isSkinned = [&](int n) {
            const int s = model.nodes[n].skin;
            return s >= 0 && s < (int)model.skins.size() && !model.skins[s].joints.empty();
            };

        // メッシュを持つノードを集める（スキン付き・スキン無しの両方）
        std::vector<int> meshNodes;
        for (int i = 0; i < nodeCount; ++i) {
            const int m = model.nodes[i].mesh;
            if (m >= 0 && m < (int)model.meshes.size()) meshNodes.push_back(i);
        }
        if (meshNodes.empty()) return false;

        // 親マップ（node -> node）
        std::vector<int> nodeParent(nodeCount, -1);
        for (int i = 0; i < nodeCount; ++i)
            for (int c : model.nodes[i].children)
                if (c >= 0 && c < nodeCount && c != i) nodeParent[c] = i;

        // ---- ボーンにするノードを決める ----
        //   ・スキンのジョイント
        //   ・スキン無しメッシュのノード（そのノード自身に 100% 付いている、と考える）
        //   ・上の2つの親をルートまで全部（Armature などの回転・スケールを落とさないため）
        std::vector<char> keep(nodeCount, 0);
        for (int mn : meshNodes) {
            if (isSkinned(mn)) {
                for (int j : model.skins[model.nodes[mn].skin].joints)
                    if (j >= 0 && j < nodeCount) keep[j] = 1;
            }
            else keep[mn] = 1;
        }
        for (int i = 0; i < nodeCount; ++i) {
            if (!keep[i]) continue;
            int guard = 0;
            for (int p = nodeParent[i]; p >= 0 && guard < nodeCount; p = nodeParent[p], ++guard) {
                if (keep[p] == 2) break;        // ここから上は確認済み
                if (!keep[p]) keep[p] = 2;      // 2 = 親として追加したもの
            }
        }

        // ---- 親が先に来る順でボーンを並べる（node index -> bone index）----
        out.skeleton.bones.clear();
        out.skeleton.joints.clear();
        std::vector<int> nodeToBone(nodeCount, -1);
        {
            std::vector<int> stack;
            for (int i = nodeCount - 1; i >= 0; --i) if (nodeParent[i] < 0) stack.push_back(i);
            std::vector<char> visited(nodeCount, 0);
            while (!stack.empty()) {
                const int n = stack.back(); stack.pop_back();
                if (visited[n]) continue;
                visited[n] = 1;
                if (keep[n]) {
                    SkeletonBone bone;
                    bone.name = model.nodes[n].name.empty() ? ("node" + std::to_string(n)) : model.nodes[n].name;
                    NodeTRS(model.nodes[n], bone.t, bone.r, bone.s);
                    const int p = nodeParent[n];
                    bone.parent = (p >= 0) ? nodeToBone[p] : -1;   // 親は keep 済みなので、先に登録されている
                    nodeToBone[n] = (int)out.skeleton.bones.size();
                    out.skeleton.bones.push_back(bone);
                }
                const auto& ch = model.nodes[n].children;
                for (int k = (int)ch.size() - 1; k >= 0; --k)
                    if (ch[k] >= 0 && ch[k] < nodeCount) stack.push_back(ch[k]);
            }
        }

        if (out.skeleton.bones.empty()) return false;

        // ---- ジョイント（ボーン＋逆バインド行列の組）は、頂点が実際に使うものだけ登録する ----
        std::vector<std::vector<int>> jointsOfBone(out.skeleton.bones.size());
        auto getJoint = [&](int bone, const XMFLOAT4X4& ibm) -> uint32_t {
            if (bone < 0) bone = 0;
            for (int j : jointsOfBone[bone])
                if (SameMatrix(out.skeleton.joints[j].inverseBind, ibm)) return (uint32_t)j;
            SkinJoint sj; sj.bone = bone; sj.inverseBind = ibm;
            const int idx = (int)out.skeleton.joints.size();
            out.skeleton.joints.push_back(sj);
            jointsOfBone[bone].push_back(idx);
            return (uint32_t)idx;
            };
        XMFLOAT4X4 identity; XMStoreFloat4x4(&identity, XMMatrixIdentity());

        std::unordered_map<int, int> imageToTexture;

        // ---- メッシュを1つにまとめる ----
        for (int mn : meshNodes) {
            const tinygltf::Node& node = model.nodes[mn];
            const bool skinned = isSkinned(mn);

            // このスキンのジョイントごとの逆バインド行列
            std::vector<XMFLOAT4X4> ibms;
            std::vector<int> localToJoint;      // スキン内の番号 -> skeleton.joints（-1 = まだ使われていない）
            if (skinned) {
                const tinygltf::Skin& skin = model.skins[node.skin];
                ibms.assign(skin.joints.size(), identity);
                localToJoint.assign(skin.joints.size(), -1);
                if (skin.inverseBindMatrices >= 0 && skin.inverseBindMatrices < (int)model.accessors.size()) {
                    const auto& a = model.accessors[skin.inverseBindMatrices];
                    size_t stride = 0; const unsigned char* d = AccPtr(model, a, stride); if (!stride) stride = sizeof(float) * 16;
                    if (d && a.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) {
                        const size_t cnt = (a.count < ibms.size()) ? a.count : ibms.size();
                        for (size_t b = 0; b < cnt; ++b) {
                            const float* m = reinterpret_cast<const float*>(d + stride * b);
                            for (int i = 0; i < 4; ++i) for (int j = 0; j < 4; ++j) ibms[b].m[i][j] = m[i * 4 + j]; // 列優先→行ベクトル形式
                        }
                    }
                }
            }
            auto skinJoint = [&](uint32_t local) -> uint32_t {
                const tinygltf::Skin& skin = model.skins[node.skin];
                if (local >= localToJoint.size()) local = 0;
                if (localToJoint[local] < 0) {
                    const int jn = skin.joints[local];
                    const int bone = (jn >= 0 && jn < nodeCount) ? nodeToBone[jn] : 0;
                    localToJoint[local] = (int)getJoint(bone, ibms[local]);
                }
                return (uint32_t)localToJoint[local];
                };
            // スキン無し：このノードに 100%。頂点はメッシュのローカル座標のままなので、逆バインドは単位行列
            const uint32_t rigidJoint = skinned ? 0u : getJoint(nodeToBone[mn], identity);

            const tinygltf::Mesh& mesh = model.meshes[node.mesh];
            for (const auto& prim : mesh.primitives) {
                if (prim.mode != TINYGLTF_MODE_TRIANGLES) continue;
                auto itPos = prim.attributes.find("POSITION");
                if (itPos == prim.attributes.end()) continue;

                const auto& posAcc = model.accessors[itPos->second];
                size_t posStride = 0; const unsigned char* posData = AccPtr(model, posAcc, posStride);
                if (!posData || posAcc.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT) continue;
                if (posStride == 0) posStride = sizeof(float) * 3;

                const unsigned char* nrmData = nullptr; size_t nrmStride = 0;
                if (auto it = prim.attributes.find("NORMAL"); it != prim.attributes.end()) { const auto& a = model.accessors[it->second]; if (a.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) { nrmData = AccPtr(model, a, nrmStride); if (!nrmStride) nrmStride = sizeof(float) * 3; } }
                const unsigned char* uvData = nullptr; size_t uvStride = 0;
                if (auto it = prim.attributes.find("TEXCOORD_0"); it != prim.attributes.end()) { const auto& a = model.accessors[it->second]; if (a.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT) { uvData = AccPtr(model, a, uvStride); if (!uvStride) uvStride = sizeof(float) * 2; } }
                const unsigned char* jData = nullptr; size_t jStride = 0; int jType = 0;
                const unsigned char* wData = nullptr; size_t wStride = 0; int wType = 0; bool wNorm = false;
                if (skinned) {
                    if (auto it = prim.attributes.find("JOINTS_0"); it != prim.attributes.end()) { const auto& a = model.accessors[it->second]; jData = AccPtr(model, a, jStride); jType = a.componentType; if (!jStride) jStride = (jType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ? 4 : 8); }
                    if (auto it = prim.attributes.find("WEIGHTS_0"); it != prim.attributes.end()) { const auto& a = model.accessors[it->second]; wData = AccPtr(model, a, wStride); wType = a.componentType; wNorm = a.normalized; if (!wStride) wStride = (wType == TINYGLTF_COMPONENT_TYPE_FLOAT ? 16 : (wType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ? 8 : 4)); }
                }

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

                    if (!skinned) {
                        v.bones[0] = v.bones[1] = v.bones[2] = v.bones[3] = rigidJoint;
                        v.weights[0] = 1; v.weights[1] = v.weights[2] = v.weights[3] = 0;
                    }
                    else if (!jData) {
                        // スキンはあるのに JOINTS_0 が無い：スキンの先頭のジョイントに 100%
                        const uint32_t j0 = skinJoint(0);
                        v.bones[0] = v.bones[1] = v.bones[2] = v.bones[3] = j0;
                        v.weights[0] = 1; v.weights[1] = v.weights[2] = v.weights[3] = 0;
                    }
                    else {
                        float w[4] = { 1,0,0,0 };
                        if (wData) { const unsigned char* e = wData + wStride * i; for (int k = 0; k < 4; ++k) w[k] = ReadCompF(e, wType, k, wNorm); }
                        float sum = w[0] + w[1] + w[2] + w[3];
                        if (sum <= 1e-6f) { w[0] = 1; w[1] = w[2] = w[3] = 0; sum = 1; }
                        const unsigned char* e = jData + jStride * i;
                        uint32_t firstUsed = 0; bool haveFirst = false;
                        for (int k = 0; k < 4; ++k) {
                            v.weights[k] = w[k] / sum;
                            if (w[k] > 0.0f) {
                                v.bones[k] = skinJoint(ReadIndexU32(e, jType, k));
                                if (!haveFirst) { firstUsed = v.bones[k]; haveFirst = true; }
                            }
                        }
                        // ウェイト 0 の枠は、使っているジョイントと同じ番号にしておく（範囲外を指さないように）
                        for (int k = 0; k < 4; ++k) if (!(w[k] > 0.0f)) v.bones[k] = firstUsed;
                    }
                    out.vertices.push_back(v);
                }

                uint32_t idxStart = (uint32_t)out.indices.size();
                bool indexed = false;
                if (prim.indices >= 0 && prim.indices < (int)model.accessors.size()) {
                    const auto& ia = model.accessors[prim.indices];
                    size_t is = 0; const unsigned char* id = AccPtr(model, ia, is);
                    if (id) {
                        if (!is) is = (ia.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT ? 4 : (ia.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ? 2 : 1));
                        for (size_t i = 0; i < ia.count; ++i) { const unsigned char* e = id + is * i; out.indices.push_back(baseVertex + ReadIndexU32(e, ia.componentType, 0)); }
                        indexed = true;
                    }
                }
                if (!indexed) {
                    for (size_t i = 0; i < posAcc.count; ++i) out.indices.push_back(baseVertex + (uint32_t)i);
                }

                SkinnedSubmesh sm;
                sm.indexOffset = idxStart;
                sm.indexCount = (uint32_t)out.indices.size() - idxStart;
                sm.name = node.name.empty() ? mesh.name : node.name;
                sm.texture = LoadTexture(model, prim.material, out, imageToTexture);
                out.submeshes.push_back(sm);
            }
        }

        // ---- アニメーション（動かすノード -> ボーン）----
        // トラックが1本も無いクリップも入れる（Scene に保存した clipIndex がずれないように、ファイルの順番のまま）
        auto resolve = [&](int n) -> int { return (n >= 0 && n < nodeCount) ? nodeToBone[n] : -1; };
        for (const auto& anim : model.animations) {
            AnimationClip clip = ReadClip(model, anim, resolve);
            if (clip.name.empty()) clip.name = "clip" + std::to_string(out.clips.size());
            out.clips.push_back(std::move(clip));
        }

        return out.Valid();
    }

    bool LoadAnimationsFromFile(const std::string& path, SkinnedModel& target)
    {
        if (target.skeleton.bones.empty()) return false;
        tinygltf::TinyGLTF loader; tinygltf::Model model; std::string err, warn;
        loader.SetImageLoader(SkipImage, nullptr);
        if (!loader.LoadBinaryFromFile(&model, &err, &warn, path)) return false;

        // 別ファイルのアニメは、ノード名 = ボーン名 で対応付ける
        auto resolve = [&](int n) -> int {
            if (n < 0 || n >= (int)model.nodes.size()) return -1;
            return target.skeleton.Find(model.nodes[n].name);
            };

        bool added = false;
        for (const auto& anim : model.animations) {
            AnimationClip clip = ReadClip(model, anim, resolve);
            if (clip.tracks.empty()) continue;
            if (clip.name.empty()) clip.name = "clip" + std::to_string(target.clips.size());
            target.clips.push_back(std::move(clip));
            added = true;
        }
        return added;
    }
} // namespace uel