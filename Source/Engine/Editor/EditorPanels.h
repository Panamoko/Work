#pragma once
#include "Engine/Scene/EntityId.h"
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
        std::string& renameSceneRequested);
    void DrawAssetBrowserPanel(World& world, EntityId& selected, CommandHistory& history);
    void DrawAnimationEditorPanel(World& world, EntityId selected, ID3D11Device* device);

    struct SceneLighting;   // 前方宣言
    struct PostProcessSettings;
    void DrawEnvironmentPanel(SceneLighting& lighting, PostProcessSettings& post);
    void DrawNavGridPanel(World& world);

    // Inspectorで Apply された Prefab のパス（Engineが全Sceneへ反映する）。取り出すと空に戻る
    std::string ConsumePrefabRefreshRequest();


}