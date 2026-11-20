#pragma once
#include "Engine/Graphics/RenderTexture.h"
#include "Engine/Graphics/PostProcess.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Graphics/MeshShader.h"
#include "Engine/Editor/TransformEdit.h"
#include "Engine/Editor/EditorCamera.h"
#include "Engine/Scene/EntityId.h"
#include "Engine/Graphics/EnvironmentMap.h"

#include <DirectXMath.h>
#include <memory>
#include "Engine/Graphics/EngineSkybox.h" 
#include "Engine/Graphics/EngineShadowMap.h"
#include "Engine/Graphics/SkinnedMeshShader.h"
#include "Engine/AI/EngineNavGrid.h"
#include "Engine/Game/GameFlow.h"

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace uel {
    class World;
    class MeshCache;   // 前方宣言
    struct SceneLighting;

    class ViewportPanel {
    public:
        void Initialize(ID3D11Device* device);
        void DrawUI(ID3D11Device* device, World& world, EntityId& selected, CommandHistory& history,
            bool playMode, MeshCache& cache);
        void RenderScene(ID3D11DeviceContext* context, World& world, MeshCache& cache,
            ID3D11Device* device, const SceneLighting& lighting, const PostProcessSettings& postSettings);
        GameFlow& GetFlow() { return flow; }
        // 経路探索のグリッドを、次のフレームで作り直す（5-4c。Engine が Scene を読み込んだときに呼ぶ）
        void RequestNavRebake() { navDirty = true; }
        // シェーダを作り直す（ホットリロードの後に呼ぶ）。作り直せなかったものは前のまま使う。全部できたら true
        bool ReloadShaders(ID3D11Device* device);
    private:
        RenderTexture rt;          // シーンを描く先
        RenderTexture finalRT;     // ポストプロセス後の画像（Viewport に表示するのはこちら）
        PostProcess   postProcess;
        MeshShader    shader;
        std::shared_ptr<Mesh> cubeMesh;

        EngineSkybox        skybox;
        EngineShadowMap shadowMap;
        EnvironmentMap  envMap;      // 周りの景色（空の画像）
        SkinnedMeshShader skinnedShader;
        EngineNavGrid nav;
        bool navDirty = true;           // グリッドを作り直す必要がある（Play の始め・Scene が変わったとき）。毎フレームは作らない
        GameFlow flow;

        int viewWidth = 16;
        int viewHeight = 16;

        DirectX::XMFLOAT4X4 view{};
        DirectX::XMFLOAT4X4 proj{};
        int gizmoOp = 0;
        TransformEditSession gizmoSession;
        EditorCamera camera;

        bool  showGrid = false;
        bool  snapEnabled = false;
        bool  showHitboxes = false;
        float snapTranslate = 1.0f;
        float snapRotate = 15.0f;
        float snapScale = 0.25f;
        bool  mouseCaptured = false;      // Play 中、マウスを Viewport に固定している（3-1。カーソルを隠して真ん中へ戻す）
        bool  captureClickHeld = false;   // マウスを固定したときの左クリックを、まだ押している（離すまで撃たない。3-3）
    };

} // namespace uel