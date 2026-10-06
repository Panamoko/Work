#pragma once
#include "Engine/Graphics/RenderTexture.h"
#include "Engine/Graphics/PostProcess.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Graphics/MeshShader.h"
#include "Engine/Editor/TransformEdit.h"
#include "Engine/Editor/EditorCamera.h"
#include "Engine/Scene/EntityId.h"
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

    private:
        RenderTexture rt;          // シーンを描く先
        RenderTexture finalRT;     // ポストプロセス後の画像（Viewport に表示するのはこちら）
        PostProcess   postProcess;
        MeshShader    shader;
        std::shared_ptr<Mesh> cubeMesh;

        EngineSkybox        skybox;
        EngineShadowMap shadowMap;
        SkinnedMeshShader skinnedShader;
        EngineNavGrid nav;
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

    };

} // namespace uel