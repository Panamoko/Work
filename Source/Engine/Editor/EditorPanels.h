#pragma once
#include "Engine/Scene/EntityId.h"
#include "Engine/Animation/AnimNode.h"     // DrawNotifyEditor が AnimClipNode::Notify を使う
#include <vector>
#include <string>
struct ID3D11Device;   // ★ グローバル名前空間で前方宣言（uelの外）

namespace uel {
    class World;
    class CommandHistory;
    struct TransformEditSession;
    class MeshCache;       // これは uel の中でOK（MeshCache は uel の型）

    void DrawHierarchyPanel(World& world, EntityId& selected, CommandHistory& history);
    void DrawInspectorPanel(World& world, EntityId selected, CommandHistory& history,
        TransformEditSession& session, MeshCache& cache, ID3D11Device* device);
    void HandleShortcuts(CommandHistory& history);
    void DrawMainMenuBar(World& world, EntityId& selected, CommandHistory& history,
        bool playing, bool& togglePlayRequested,
        const std::vector<std::string>& sceneNames, int& activeScene, bool& addSceneRequested,
        std::string& renameSceneRequested, bool& sceneSaved);
    void DrawAssetBrowserPanel(World& world, EntityId& selected, CommandHistory& history);
    void DrawAnimationEditorPanel(World& world, EntityId selected, ID3D11Device* device);

    struct SceneLighting;   // 前方宣言
    struct PostProcessSettings;
    void DrawEnvironmentPanel(SceneLighting& lighting, PostProcessSettings& post);
    void DrawNavGridPanel(World& world);

    // Inspectorで Apply された Prefab のパス（Engineが全Sceneへ反映する）。取り出すと空に戻る
    std::string ConsumePrefabRefreshRequest();

    class AnimNode;
    class AnimParams;
    struct SkinnedModel;
    void DrawAnimNodeDetails(AnimNode* node, const SkinnedModel& model, AnimParams& params);   // ノード1つぶんの編集欄（ノードエディタから呼ぶ）
    void DrawNotifyEditor(std::vector<AnimClipNode::Notify>& notifies, float duration, float playTime);   // 通知のタイムライン＋一覧（playTime < 0 なら赤い線を出さない）
    void RequestAssetRescan();     // Asset Browser に、次のフレームで一覧を作り直させる（ファイルを保存した後に呼ぶ）

}