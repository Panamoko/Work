#include "Engine/Editor/AnimGraphPanel.h"
#include "Engine/Editor/AnimGraphPanelInternal.h"
#include "Engine/Editor/EditorPanels.h"
#include "Engine/Scene/World.h"
#include "Engine/Components/AnimatorComponent.h"
#include "Engine/Components/SkinnedMeshComponent.h"
#include "Engine/Assets/SkinnedModelCache.h"
#include "Engine/Animation/AnimBlendSpaceNode.h"
#include "Engine/Serialization/AnimGraphSerializer.h"

#include <imgui.h>
#include <cstring>
#include <memory>
#include <string>

// Anim Graph パネルの入口（DrawAnimGraphPanel）と、上のアセットの行・左のパラメータの欄。
// 階層の説明・共有の状態・定数は AnimGraphPanelInternal.h、各階層の表示は AnimGraphCanvas.cpp と AnimGraphStateMachineView.cpp にある。
namespace uel {
    using namespace animgraph;     // 共有の状態 g・定数・補助関数を名前だけで使う（このファイルの中だけ）

    namespace {
        // パラメータの名前を変えたとき、その名前を使っている所（条件、Blend Space の軸、再生速度）も書き換える
        void RenameParamRefs(AnimNode* n, const std::string& oldName, const std::string& newName)
        {
            if (!n) return;
            if (auto* cn = dynamic_cast<AnimClipNode*>(n)) {
                if (cn->speedParam == oldName) cn->speedParam = newName;
            }
            else if (auto* bs = dynamic_cast<AnimBlendSpace1DNode*>(n)) {
                if (bs->param == oldName) bs->param = newName;
            }
            else if (auto* sm = dynamic_cast<SM*>(n)) {
                for (SM::Transition& tr : sm->transitions)
                    for (SM::Condition& c : tr.conditions) if (c.param == oldName) c.param = newName;
                for (SM::State& st : sm->states) RenameParamRefs(st.node.get(), oldName, newName);
            }
        }

        // =====================================================================
        //  上の行：グラフのアセット（保存・読み込み）。読み込んだら true（ノードが作り直された）
        // =====================================================================
        bool DrawAssetRow(AnimatorComponent* an)
        {
            static std::string graphMsg;
            static float       graphMsgTimer = 0.0f;
            bool reloaded = false;

            char gb[200]; strncpy_s(gb, sizeof(gb), an->graphPath.c_str(), _TRUNCATE);
            ImGui::SetNextItemWidth(340.0f);
            if (ImGui::InputText("Graph Asset", gb, sizeof(gb))) an->graphPath = gb;       // 名前だけでもよい（例：Enemy）
            if (ImGui::BeginDragDropTarget()) {                     // Asset Browser の [Graph] をここへドロップすると読み込む
                if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("ASSET_ANIMGRAPH")) {
                    if (pl->Data) {
                        const std::string path = static_cast<const char*>(pl->Data);
                        const bool ok = LoadAnimGraph(path, an->params, an->graph, an->montages);
                        if (ok) { an->graphPath = path; reloaded = true; }
                        graphMsg = ok ? "Loaded" : "Load failed (file?)";
                        graphMsgTimer = 3.0f;
                    }
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::SameLine();
            if (ImGui::Button("Save")) {
                const std::string path = AnimGraphPathFromName(an->graphPath);
                const bool ok = an->graph && !path.empty() && SaveAnimGraph(path, an->params, an->graph.get(), an->montages);
                if (ok) { an->graphPath = path; RequestAssetRescan(); }
                graphMsg = ok ? "Saved" : (an->graph ? "Save failed (name?)" : "No graph to save");
                graphMsgTimer = 3.0f;
            }
            ImGui::SameLine();
            if (ImGui::Button("Load")) {
                const std::string path = AnimGraphPathFromName(an->graphPath);
                const bool ok = !path.empty() && LoadAnimGraph(path, an->params, an->graph, an->montages);
                if (ok) { an->graphPath = path; reloaded = true; }
                graphMsg = ok ? "Loaded" : "Load failed (file?)";
                graphMsgTimer = 3.0f;
            }
            ImGui::SameLine();
            if (graphMsgTimer > 0.0f) {
                graphMsgTimer -= ImGui::GetIO().DeltaTime;
                ImGui::TextUnformatted(graphMsg.c_str());
            }
            else ImGui::TextDisabled("(edits are kept only after Save)");
            return reloaded;
        }

        // =====================================================================
        //  左の欄：パラメータ（グラフに渡す値）
        // =====================================================================
        void DrawParamsPane(AnimatorComponent* an, const SkinnedModel* model)
        {
            ImGui::BeginChild("ag_params", ImVec2(kParamsW, 0.0f), true);
            ImGui::TextUnformatted("Parameters");
            ImGui::Separator();
            const char* typeNames[] = { "Float", "Bool", "Trigger" };   // same order as AnimParamType
            int removeAt = -1;
            if (g.renameIndex >= (int)an->params.items.size()) g.renameIndex = -1;
            for (int i = 0; i < (int)an->params.items.size(); ++i) {
                AnimParam& p = an->params.items[i];
                ImGui::PushID(i);
                int ty = (int)p.type;
                ImGui::SetNextItemWidth(66);
                if (ImGui::Combo("##type", &ty, typeNames, 3)) { p.type = (AnimParamType)ty; p.value = 0.0f; }
                ImGui::SameLine();

                // 名前：打っている途中は g.renameBuf に入れておき、打ち終わったとき（Enter か、欄の外をクリック）に反映する。
                // 1文字ごとに反映すると、その名前を使っている条件などが、途中の名前を追いかけきれないため。
                char nb[64]; strncpy_s(nb, sizeof(nb), p.name.c_str(), _TRUNCATE);
                char* buf = (g.renameIndex == i) ? g.renameBuf : nb;
                ImGui::SetNextItemWidth(78);
                ImGui::InputText("##name", buf, sizeof(nb));
                if (ImGui::IsItemActivated()) { g.renameIndex = i; strncpy_s(g.renameBuf, sizeof(g.renameBuf), p.name.c_str(), _TRUNCATE); }
                if (g.renameIndex == i && ImGui::IsItemDeactivated()) {
                    const std::string oldName = p.name, newName = g.renameBuf;
                    bool taken = false;     // 空の名前と、ほかのパラメータと同じ名前は受け付けない
                    for (int k = 0; k < (int)an->params.items.size(); ++k) if (k != i && an->params.items[k].name == newName) taken = true;
                    if (!newName.empty() && !taken && newName != oldName) {
                        p.name = newName;
                        RenameParamRefs(an->graph.get(), oldName, newName);     // 条件や Blend Space の軸も、新しい名前に付け替える
                    }
                    g.renameIndex = -1;
                }

                ImGui::SameLine();
                if (p.type == AnimParamType::Float) {
                    ImGui::SetNextItemWidth(52);
                    ImGui::DragFloat("##val", &p.value, 0.01f, 0.0f, 0.0f, "%.2f");
                }
                else if (p.type == AnimParamType::Bool) {
                    bool b = p.value > 0.5f;
                    if (ImGui::Checkbox("##val", &b)) p.value = b ? 1.0f : 0.0f;
                }
                else {
                    if (ImGui::SmallButton("Fire")) p.value = 1.0f;     // このフレームだけ立つ
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("x")) removeAt = i;
                ImGui::PopID();
            }
            if (removeAt >= 0) { an->params.items.erase(an->params.items.begin() + removeAt); g.renameIndex = -1; }
            if (ImGui::SmallButton("+ Parameter")) {
                // ほかと重ならない名前を付ける
                std::string name;
                for (int n = (int)an->params.items.size(); ; ++n) {
                    name = "Param" + std::to_string(n);
                    if (an->params.Find(name) < 0) break;
                }
                AnimParam p; p.name = name;
                an->params.items.push_back(p);
            }
            ImGui::Spacing();
            ImGui::TextDisabled("Float : compare with > or <");
            ImGui::TextDisabled("Bool  : true / false");
            ImGui::TextDisabled("Trigger : fires once");
            ImGui::Spacing();
            if (!an->lastEvent.empty() && an->lastEventAge < 1.0f) ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.35f, 1.0f), "Notify: %s", an->lastEvent.c_str());
            else ImGui::TextDisabled("Notify: -");

            // ---- モンタージュ：グラフの上に、1回だけ重ねて再生するクリップ（ゲーム側は PlayMontage("名前") で呼ぶ）----
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextUnformatted("Montages");
            ImGui::PushID("montages");      // パラメータの行と ID が重ならないように
            static int montageSel = -1;     // 編集欄を開いているモンタージュの番号（-1 = 閉じている）
            if (montageSel >= (int)an->montages.size()) montageSel = -1;
            int removeM = -1;
            for (int i = 0; i < (int)an->montages.size(); ++i) {
                AnimMontage& mt = an->montages[i];
                ImGui::PushID(i);
                const bool playingThis = an->IsMontagePlaying(mt.name);
                std::string label = (mt.name.empty() ? std::string("(no name)") : mt.name) + (playingThis ? "  *" : "");
                if (ImGui::Selectable(label.c_str(), i == montageSel, 0, ImVec2(130.0f, 0.0f))) montageSel = (i == montageSel) ? -1 : i;
                ImGui::SameLine();
                if (ImGui::SmallButton("Play")) an->PlayMontage(mt);
                ImGui::SameLine();
                if (ImGui::SmallButton("x")) removeM = i;
                ImGui::PopID();
            }
            if (removeM >= 0) { an->montages.erase(an->montages.begin() + removeM); montageSel = -1; }
            if (ImGui::SmallButton("+ Montage")) {
                AnimMontage mt;
                for (int n = (int)an->montages.size(); ; ++n) {     // ほかと重ならない名前を付ける
                    mt.name = "Montage" + std::to_string(n);
                    if (!an->FindMontage(mt.name)) break;
                }
                if (model && !model->clips.empty()) mt.clipName = model->clips[0].name;
                an->montages.push_back(mt);
                montageSel = (int)an->montages.size() - 1;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Stop")) an->StopMontage();

            // 再生中のモンタージュ
            const AnimatorComponent::MontageState& ms = an->montage;
            if (ms.active) ImGui::Text("%s%s  %.2f s  w %.2f", ms.name.empty() ? ms.clip.clipName.c_str() : ms.name.c_str(), ms.stopping ? " (out)" : "", ms.clip.time, ms.weight);
            else ImGui::TextDisabled("not playing");

            // 選んでいるモンタージュの編集欄
            if (montageSel >= 0) {
                AnimMontage& mt = an->montages[montageSel];
                ImGui::Separator();
                char nb[64]; strncpy_s(nb, sizeof(nb), mt.name.c_str(), _TRUNCATE);
                ImGui::SetNextItemWidth(150.0f);
                if (ImGui::InputText("Name", nb, sizeof(nb))) mt.name = nb;
                ImGui::SetNextItemWidth(150.0f);
                if (ImGui::BeginCombo("Clip", mt.clipName.empty() ? "(none)" : mt.clipName.c_str())) {
                    if (model) {
                        for (int i = 0; i < (int)model->clips.size(); ++i) {
                            ImGui::PushID(i);
                            if (ImGui::Selectable(model->clips[i].name.c_str(), model->clips[i].name == mt.clipName)) mt.clipName = model->clips[i].name;
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndCombo();
                }
                ImGui::SetNextItemWidth(150.0f);
                ImGui::DragFloat("Blend In", &mt.blendIn, 0.01f, 0.0f, 2.0f, "%.2f s");
                ImGui::SetNextItemWidth(150.0f);
                ImGui::DragFloat("Blend Out", &mt.blendOut, 0.01f, 0.0f, 2.0f, "%.2f s");
                ImGui::SetNextItemWidth(150.0f);
                ImGui::DragFloat("Rate", &mt.rate, 0.01f, 0.05f, 4.0f, "%.2f");

                // クリップの長さ（タイムラインの横幅）と、このモンタージュを再生中ならその位置
                float dur = 0.0f;
                if (model) for (const AnimationClip& c : model->clips) if (c.name == mt.clipName) { dur = c.duration; break; }
                if (dur <= 0.0f) ImGui::TextDisabled("clip not found in this model");
                else ImGui::TextDisabled("length %.2f s", dur);
                const float playTime = an->IsMontagePlaying(mt.name) ? ms.clip.time : -1.0f;
                ImGui::TextUnformatted("Notifies");
                DrawNotifyEditor(mt.notifies, dur, playTime);
                ImGui::TextDisabled("(changes apply from the next Play)");
            }
            ImGui::PopID();
            ImGui::EndChild();
        }

    } // namespace

    void DrawAnimGraphPanel(World& world, EntityId selected, bool playMode, ID3D11Device* device)
    {
        ImGui::Begin("Anim Graph");

        Entity* e = world.Get(selected);
        AnimatorComponent* an = e ? e->GetComponent<AnimatorComponent>() : nullptr;
        if (!an) { ImGui::TextUnformatted("Select an Entity that has an AnimatorComponent."); ImGui::End(); return; }

        // クリップの名前を選ぶために、同じ Entity のモデルを取っておく（無ければ nullptr）
        std::shared_ptr<SkinnedRenderModel> modelHolder;
        if (SkinnedMeshComponent* sk = e->GetComponent<SkinnedMeshComponent>())
            if (!sk->assetPath.empty()) modelHolder = GlobalSkinnedCache().GetOrLoad(device, sk->assetPath, sk->animPath);
        const SkinnedModel* model = (modelHolder && modelHolder->Valid()) ? &modelHolder->Cpu() : nullptr;

        // 別の Entity を選んだら、選択も、見ている階層も、最初に戻す
        if (g.lastEntity != selected) { g.lastEntity = selected; g.ResetForNewGraph(); }

        // ---- 上の1行目：グラフのアセット ----
        if (DrawAssetRow(an)) {
            // 読み込むとノードが作り直される（今まで見ていたノードは無くなる）ので、このフレームはここで終える
            g.ResetForNewGraph();
            ImGui::End();
            return;
        }

        // ---- 見ている階層が、今のグラフでまだ成り立つかを確かめる ----
        SM* rootSM = dynamic_cast<SM*>(an->graph.get());
        if (!rootSM) g.level = Level::AnimGraph;                                    // 根がステートマシンでなければ、中には入れない
        if (g.level == Level::State && (g.levelState < 0 || g.levelState >= (int)rootSM->states.size())) g.level = Level::StateMachine;

        // ---- 上の2行目：いまの場所（押すと、その階層へ戻る）と、表示の操作 ----
        {
            if (ImGui::SmallButton("AnimGraph")) g.level = Level::AnimGraph;
            if (rootSM && g.level != Level::AnimGraph) {
                ImGui::SameLine(); ImGui::TextDisabled(">"); ImGui::SameLine();
                if (ImGui::SmallButton("State Machine")) g.level = Level::StateMachine;
            }
            if (rootSM && g.level == Level::State) {
                ImGui::SameLine(); ImGui::TextDisabled(">"); ImGui::SameLine();
                ImGui::Text("State: %s", rootSM->states[g.levelState].name.c_str());
            }

            ImGui::SameLine(); ImGui::TextDisabled("  |");
            ImGui::SameLine();
            if (ImGui::SmallButton("Fit")) g.fitRequested = true;
            ImGui::SameLine();
            const float curZoom = (g.level == Level::StateMachine) ? g.zoom : g.poseZoom;
            ImGui::TextDisabled("%d%%", (int)(curZoom * 100.0f + 0.5f));

            ImGui::SameLine();
            if (g.level == Level::StateMachine) {
                const bool nowValid = (rootSM->current >= 0 && rootSM->current < (int)rootSM->states.size());
                ImGui::Text("   Now: %s", nowValid ? rootSM->states[rootSM->current].name.c_str() : "-");
                if (g.linkFrom != kNoLink) { ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.35f, 1.0f), "   Click the target state  (Esc / right-click: cancel)"); }
            }
            else ImGui::TextUnformatted("");
            if (playMode) { ImGui::SameLine(); ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "  PLAY (live, edits are not kept)"); }
        }

        // ---- 左：パラメータ ----
        DrawParamsPane(an, model);
        ImGui::SameLine();

        // ---- 中央と右：見ている階層 ----
        if (g.level == Level::StateMachine) {
            const int open = DrawStateMachineView(an, rootSM, model);
            if (open >= 0 && open < (int)rootSM->states.size()) { g.level = Level::State; g.levelState = open; g.poseSelected = true; }
        }
        else if (g.level == Level::State) {
            DrawPoseView(an, rootSM->states[g.levelState].node, false, model);     // 状態の中には、ステートマシンを置かない
        }
        else {
            if (DrawPoseView(an, an->graph, true, model)) { g.level = Level::StateMachine; g.ClearSelection(); g.fitRequested = true; }
        }

        ImGui::End();
    }

} // namespace uel
