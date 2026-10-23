#include "Engine/Serialization/AnimGraphSerializer.h"
#include "Engine/Animation/AnimBlendSpaceNode.h"
#include "Engine/Animation/AnimStateMachineNode.h"
#include "Engine/Config/EngineConfig.h"

#include <fstream>
#include <filesystem>
#include <vector>
#include <string>
#include <cereal/archives/json.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/string.hpp>

namespace uel {
    namespace {

        // ファイルの版。項目を足すときは +1 して、読む側で「その版以降だけ読む」ようにする
        //   1 … 最初の形
        //   2 … ノードエディタ上の位置（状態の x / y、Any の箱の anyX / anyY）
        //   3 … クリップの通知（notifies）
        //   4 … モンタージュの定義（montages）
        //   5 … モンタージュの重ねる範囲（maskBone / maskBlendDepth）
        constexpr int kAnimGraphVersion = 5;
        int g_loadVersion = kAnimGraphVersion;     // 読み込み中のファイルの版（保存するときは最新）

        struct ParamDTO {
            std::string name; int type = 0; float value = 0.0f;
            template<class A> void serialize(A& ar) { ar(CEREAL_NVP(name), CEREAL_NVP(type), CEREAL_NVP(value)); }
        };
        struct SampleDTO {
            std::string clip; float position = 0.0f;
            template<class A> void serialize(A& ar) { ar(CEREAL_NVP(clip), CEREAL_NVP(position)); }
        };
        struct ConditionDTO {
            std::string param; int op = 0; float threshold = 0.0f;
            template<class A> void serialize(A& ar) { ar(CEREAL_NVP(param), CEREAL_NVP(op), CEREAL_NVP(threshold)); }
        };
        struct NotifyDTO {
            float time = 0.0f; std::string name;
            template<class A> void serialize(A& ar) { ar(CEREAL_NVP(time), CEREAL_NVP(name)); }
        };
        struct MontageDTO {
            std::string name, clip;
            float blendIn = 0.1f, blendOut = 0.2f, rate = 1.0f;
            std::string maskBone; int maskBlendDepth = 0;
            std::vector<NotifyDTO> notifies;
            template<class A> void serialize(A& ar) {
                ar(CEREAL_NVP(name), CEREAL_NVP(clip), CEREAL_NVP(blendIn), CEREAL_NVP(blendOut), CEREAL_NVP(rate), CEREAL_NVP(notifies));
                if (g_loadVersion >= 5) ar(CEREAL_NVP(maskBone), CEREAL_NVP(maskBlendDepth));
            }
        };
        struct TransitionDTO {
            int from = -1, to = 0; float blendTime = 0.2f; bool waitForEnd = false;
            std::vector<ConditionDTO> conditions;
            template<class A> void serialize(A& ar) {
                ar(CEREAL_NVP(from), CEREAL_NVP(to), CEREAL_NVP(blendTime), CEREAL_NVP(waitForEnd), CEREAL_NVP(conditions));
            }
        };

        struct NodeDTO;
        struct StateDTO {
            std::string name;
            std::vector<NodeDTO> node;      // 0個か1個（状態の中身）
            float x = 0.0f, y = 0.0f;       // ノードエディタ上の箱の位置（版 2 から）
            template<class A> void serialize(A& ar);
        };

        // ノード1つ。type を先に読み書きし、その種類の項目だけを続ける。
        struct NodeDTO {
            std::string type;               // "Clip" / "BlendSpace1D" / "StateMachine"

            // Clip
            std::string clip; bool loop = true; float speed = 1.0f; std::string speedParam;
            std::vector<NotifyDTO> notifies;
            // BlendSpace1D
            std::string param; float smoothTime = 0.1f; std::vector<SampleDTO> samples;
            // StateMachine
            std::vector<StateDTO> states; std::vector<TransitionDTO> transitions; int entry = 0;
            float anyX = -190.0f, anyY = 8.0f;      // ノードエディタ上の「Any」の箱の位置（版 2 から）

            template<class A> void serialize(A& ar) {
                ar(CEREAL_NVP(type));
                if (type == "Clip")
                {
                    ar(CEREAL_NVP(clip), CEREAL_NVP(loop), CEREAL_NVP(speed), CEREAL_NVP(speedParam));
                    if (g_loadVersion >= 3) ar(CEREAL_NVP(notifies));
                }
                else if (type == "BlendSpace1D")
                    ar(CEREAL_NVP(param), CEREAL_NVP(speed), CEREAL_NVP(smoothTime), CEREAL_NVP(samples));
                else if (type == "StateMachine")
                {
                    ar(CEREAL_NVP(entry), CEREAL_NVP(states), CEREAL_NVP(transitions));
                    if (g_loadVersion >= 2) ar(CEREAL_NVP(anyX), CEREAL_NVP(anyY));
                }
            }
        };
        template<class A> void StateDTO::serialize(A& ar) {
            ar(CEREAL_NVP(name), CEREAL_NVP(node));
            if (g_loadVersion >= 2) ar(CEREAL_NVP(x), CEREAL_NVP(y));
        }

        // ノード → 保存データ（子も含めて）
        NodeDTO ToDTO(const AnimNode& n)
        {
            NodeDTO d;
            if (auto* cn = dynamic_cast<const AnimClipNode*>(&n)) {
                d.type = "Clip";
                d.clip = cn->clipName; d.loop = cn->loop; d.speed = cn->speed; d.speedParam = cn->speedParam;
                for (const auto& nt : cn->notifies) d.notifies.push_back({ nt.time, nt.name });
            }
            else if (auto* bs = dynamic_cast<const AnimBlendSpace1DNode*>(&n)) {
                d.type = "BlendSpace1D";
                d.param = bs->param; d.speed = bs->speed; d.smoothTime = bs->smoothTime;
                for (const auto& s : bs->samples) d.samples.push_back({ s.clipName, s.position });
            }
            else if (auto* sm = dynamic_cast<const AnimStateMachineNode*>(&n)) {
                d.type = "StateMachine";
                d.entry = sm->entry;
                d.anyX = sm->anyX; d.anyY = sm->anyY;
                for (const auto& st : sm->states) {
                    StateDTO sd; sd.name = st.name;
                    sd.x = st.editorX; sd.y = st.editorY;
                    if (st.node) sd.node.push_back(ToDTO(*st.node.get()));
                    d.states.push_back(std::move(sd));
                }
                for (const auto& tr : sm->transitions) {
                    TransitionDTO td;
                    td.from = tr.from; td.to = tr.to; td.blendTime = tr.blendTime; td.waitForEnd = tr.waitForEnd;
                    for (const auto& c : tr.conditions) td.conditions.push_back({ c.param, (int)c.op, c.threshold });
                    d.transitions.push_back(std::move(td));
                }
            }
            return d;
        }

        // 保存データ → ノード（知らない種類なら空）
        AnimNodePtr FromDTO(const NodeDTO& d)
        {
            if (d.type == "Clip") {
                auto n = std::make_unique<AnimClipNode>();
                n->clipName = d.clip; n->loop = d.loop; n->speed = d.speed; n->speedParam = d.speedParam;
                for (const auto& nd : d.notifies) n->notifies.push_back({ nd.time, nd.name });
                return AnimNodePtr(std::move(n));
            }
            if (d.type == "BlendSpace1D") {
                auto n = std::make_unique<AnimBlendSpace1DNode>();
                n->param = d.param; n->speed = d.speed; n->smoothTime = d.smoothTime;
                for (const auto& s : d.samples) n->samples.push_back({ s.clip, s.position });
                return AnimNodePtr(std::move(n));
            }
            if (d.type == "StateMachine") {
                auto n = std::make_unique<AnimStateMachineNode>();
                n->entry = d.entry;
                n->anyX = d.anyX; n->anyY = d.anyY;
                for (const auto& sd : d.states) {
                    AnimStateMachineNode::State st;
                    st.name = sd.name;
                    st.editorX = sd.x; st.editorY = sd.y;
                    if (!sd.node.empty()) st.node = FromDTO(sd.node[0]);
                    n->states.push_back(std::move(st));
                }
                for (const auto& td : d.transitions) {
                    AnimStateMachineNode::Transition tr;
                    tr.from = td.from; tr.to = td.to; tr.blendTime = td.blendTime; tr.waitForEnd = td.waitForEnd;
                    for (const auto& c : td.conditions)
                        tr.conditions.push_back({ c.param, (AnimStateMachineNode::CondOp)c.op, c.threshold });
                    n->transitions.push_back(std::move(tr));
                }
                return AnimNodePtr(std::move(n));
            }
            return AnimNodePtr();
        }

    } // namespace

    std::string AnimGraphPathFromName(const std::string& nameOrPath)
    {
        if (nameOrPath.empty()) return nameOrPath;
        const bool looksLikePath = nameOrPath.find('/') != std::string::npos
            || nameOrPath.find('\\') != std::string::npos
            || nameOrPath.find(".json") != std::string::npos;
        if (looksLikePath) return nameOrPath;
        return std::string(kAssetRoot) + "/Anim/" + nameOrPath + ".animgraph.json";
    }

    bool SaveAnimGraph(const std::string& path, const AnimParams& params, const AnimNode* root, const std::vector<AnimMontage>& montages)
    {
        if (path.empty() || !root) return false;

        std::error_code ec;
        const std::filesystem::path parent = std::filesystem::path(path).parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent, ec);

        std::vector<ParamDTO> pd;
        for (const auto& p : params.items)      // Trigger は「立っている」状態を保存しない（読み込んだ直後に発火しないように）
            pd.push_back({ p.name, (int)p.type, (p.type == AnimParamType::Trigger) ? 0.0f : p.value });
        std::vector<NodeDTO> rd;        // 根は1個だけだが、「無い」も表せるように配列にしておく
        rd.push_back(ToDTO(*root));
        std::vector<MontageDTO> md;
        for (const AnimMontage& m : montages) {
            MontageDTO d;
            d.name = m.name; d.clip = m.clipName; d.blendIn = m.blendIn; d.blendOut = m.blendOut; d.rate = m.rate;
            d.maskBone = m.maskBone; d.maskBlendDepth = m.maskBlendDepth;
            for (const auto& nt : m.notifies) d.notifies.push_back({ nt.time, nt.name });
            md.push_back(std::move(d));
        }

        std::ofstream ofs(path);
        if (!ofs) return false;
        int version = kAnimGraphVersion;
        g_loadVersion = kAnimGraphVersion;      // 保存は、いつも最新の版の項目を全部書く
        cereal::JSONOutputArchive ar(ofs);
        ar(cereal::make_nvp("version", version),
            cereal::make_nvp("params", pd),
            cereal::make_nvp("root", rd),
            cereal::make_nvp("montages", md));
        return true;    // ar が破棄されるときに書き出される
    }

    bool LoadAnimGraph(const std::string& path, AnimParams& params, AnimNodePtr& root, std::vector<AnimMontage>& montages)
    {
        std::ifstream ifs(path);
        if (!ifs) return false;

        int version = 0;
        std::vector<ParamDTO> pd;
        std::vector<NodeDTO> rd;
        std::vector<MontageDTO> md;
        try {
            cereal::JSONInputArchive ar(ifs);
            ar(cereal::make_nvp("version", version));
            g_loadVersion = version;            // このファイルの版にある項目だけを読む（古いファイルもそのまま読める）
            ar(cereal::make_nvp("params", pd),
                cereal::make_nvp("root", rd));
            if (version >= 4) ar(cereal::make_nvp("montages", md));
        }
        catch (...) {
            g_loadVersion = kAnimGraphVersion;
            return false;       // JSON として読めない／必要な項目が無い
        }
        g_loadVersion = kAnimGraphVersion;
        if (rd.empty()) return false;
        AnimNodePtr loaded = FromDTO(rd[0]);
        if (!loaded) return false;

        // ここまで来たら成功。呼び出し側の値を置き換える
        params.items.clear();
        for (const auto& p : pd) {
            AnimParam ap; ap.name = p.name; ap.type = (AnimParamType)p.type; ap.value = p.value;
            params.items.push_back(ap);
        }
        root = std::move(loaded);
        montages.clear();                       // 版 3 以前のファイルには無い：空になる
        for (const MontageDTO& d : md) {
            AnimMontage m;
            m.name = d.name; m.clipName = d.clip; m.blendIn = d.blendIn; m.blendOut = d.blendOut; m.rate = d.rate;
            m.maskBone = d.maskBone; m.maskBlendDepth = d.maskBlendDepth;
            for (const auto& nd : d.notifies) m.notifies.push_back({ nd.time, nd.name });
            montages.push_back(std::move(m));
        }
        return true;
    }

} // namespace uel