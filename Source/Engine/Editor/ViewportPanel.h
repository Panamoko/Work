#pragma once
#include "Engine/Graphics/RenderTexture.h"
#include "Engine/Graphics/Mesh.h"
#include "Engine/Graphics/MeshShader.h"
#include "Engine/Editor/TransformEdit.h"
#include "Engine/Editor/EditorCamera.h"
#include "Engine/Scene/EntityId.h"
#include <DirectXMath.h>
#include <memory>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace uel {
    class World;
    class MeshCache;   // 前方宣言

    class ViewportPanel {
    public:
        void Initialize(ID3D11Device* device);
        void DrawUI(ID3D11Device* device, World& world, EntityId& selected, CommandHistory& history,
            bool playMode, MeshCache& cache);
        void RenderScene(ID3D11DeviceContext* context, World& world, MeshCache& cache, ID3D11Device* device);

    private:
        RenderTexture rt;
        MeshShader    shader;
        std::shared_ptr<Mesh> cubeMesh;
        int viewWidth = 16;
        int viewHeight = 16;

        DirectX::XMFLOAT4X4 view{};
        DirectX::XMFLOAT4X4 proj{};
        int gizmoOp = 0;
        TransformEditSession gizmoSession;
        EditorCamera camera;

        // ★グリッド／スナップ設定
        bool  showGrid = true;
        bool  snapEnabled = false;
        float snapTranslate = 1.0f;   // 移動グリッド量
        float snapRotate = 15.0f;  // 回転角度（度）
        float snapScale = 0.25f; // 拡縮倍率

        float playEyeHeight = 1.6f;
        float playEyeForward = 0.5f;

        // ★Play用カメラ状態（エンティティは動かさない）
        DirectX::XMFLOAT3 playPos{ 0.0f, 0.0f, 0.0f }; // （未使用でも可）
        float playYaw = 0.0f;
        float playPitch = 0.0f;
        bool  prevPlayMode = false;

        float playVelY = 0.0f;   // ★縦速度（重力/ジャンプ）
        float playGroundY = 0.0f;   // ★地面の高さ（足元）

    };

} // namespace uel