#pragma once
#include "Engine/Scene/EntityId.h"

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
        bool playing, bool& togglePlayRequested);
    void DrawAssetBrowserPanel(World& world, EntityId& selected, CommandHistory& history);
}