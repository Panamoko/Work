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
        constexpr int kAnimGraphVersion = 1;

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
            template<class A> void serialize(A& ar);
        };

        // ノード1つ。type を先に読み書きし、その種類の項目だけを続ける。
        struct NodeDTO {
            std::string type;               // "Clip" / "BlendSpace1D" / "StateMachine"

            // Clip
            std::string clip; bool loop = true; float speed = 1.0f; std::string speedParam;
            // BlendSpace1D
            std::string param; float smoothTime = 0.1f; std::vector<SampleDTO> samples;
            // StateMachine
            std::vector<StateDTO> states; std::vector<TransitionDTO> transitions; int entry = 0;

            template<class A> void serialize(A& ar) {
                ar(CEREAL_NVP(type));
                if (type == "Clip")
                    ar(CEREAL_NVP(clip), CEREAL_NVP(loop), CEREAL_NVP(speed), CEREAL_NVP(speedParam));
                else if (type == "BlendSpace1D")
                    ar(CEREAL_NVP(param), CEREAL_NVP(speed), CEREAL_NVP(smoothTime), CEREAL_NVP(samples));
                else if (type == "StateMachine")
                    ar(CEREAL_NVP(entry), CEREAL_NVP(states), CEREAL_NVP(transitions));
            }
        };
        template<class A> void StateDTO::serialize(A& ar) { ar(CEREAL_NVP(name), CEREAL_NVP(node)); }

        // ノード → 保存データ（子も含めて）
        NodeDTO ToDTO(const AnimNode& n)
        {
            NodeDTO d;
            if (auto* cn = dynamic_cast<const AnimClipNode*>(&n)) {
                d.type = "Clip";
                d.clip = cn->clipName; d.loop = cn->loop; d.speed = cn->speed; d.speedParam = cn->speedParam;
            }
            else if (auto* bs = dynamic_cast<const AnimBlendSpace1DNode*>(&n)) {
                d.type = "BlendSpace1D";
                d.param = bs->param; d.speed = bs->speed; d.smoothTime = bs->smoothTime;
                for (const auto& s : bs->samples) d.samples.push_back({ s.clipName, s.position });
            }
            else if (auto* sm = dynamic_cast<const AnimStateMachineNode*>(&n)) {
                d.type = "StateMachine";
                d.entry = sm->entry;
                for (const auto& st : sm->states) {
                    StateDTO sd; sd.name = st.name;
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
                for (const auto& sd : d.states) {
                    AnimStateMachineNode::State st;
                    st.name = sd.name;
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

    bool SaveAnimGraph(const std::string& path, const AnimParams& params, const AnimNode* root)
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

        std::ofstream ofs(path);
        if (!ofs) return false;
        int version = kAnimGraphVersion;
        cereal::JSONOutputArchive ar(ofs);
        ar(cereal::make_nvp("version", version),
            cereal::make_nvp("params", pd),
            cereal::make_nvp("root", rd));
        return true;    // ar が破棄されるときに書き出される
    }

    bool LoadAnimGraph(const std::string& path, AnimParams& params, AnimNodePtr& root)
    {
        std::ifstream ifs(path);
        if (!ifs) return false;

        int version = 0;
        std::vector<ParamDTO> pd;
        std::vector<NodeDTO> rd;
        try {
            cereal::JSONInputArchive ar(ifs);
            ar(cereal::make_nvp("version", version),
                cereal::make_nvp("params", pd),
                cereal::make_nvp("root", rd));
        }
        catch (...) {
            return false;       // JSON として読めない／必要な項目が無い
        }
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
        return true;
    }

} // namespace uel