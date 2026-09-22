#include "Engine/Core/Engine.h"
#include "Engine/Editor/EditorPanels.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"

namespace uel {

    Engine::~Engine()
    {
        if (window) window->SetMessageHook(nullptr); // ImGui破棄より先にフック解除
        if (imgui)  imgui->Shutdown();
    }

    bool Engine::Initialize(HINSTANCE instance, const wchar_t* title, int width, int height)
    {
        // --- ここが欠けていた：ウィンドウ／デバイス／ImGui／Viewport の生成 ---
        window = std::make_unique<Window>(instance, title, width, height);

        graphics = std::make_unique<GraphicsDevice>();
        if (!graphics->Initialize(window->GetHandle(), width, height))
            return false;

        imgui = std::make_unique<ImGuiLayer>();
        imgui->Initialize(window->GetHandle(), graphics->GetDevice(), graphics->GetContext());
        window->SetMessageHook(&ImGuiLayer::HandleMessage);   // ← CreateContext の後に設定

        viewport = std::make_unique<ViewportPanel>();
        viewport->Initialize(graphics->GetDevice());

        // --- 動作確認用のデモEntity（後で削除）---
        auto make = [&](const char* name, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 scale,
            bool withMesh, DirectX::XMFLOAT4 color) {
                EntityId id = world.CreateEntity(name);
                Entity* e = world.Get(id);
                auto* t = e->AddComponent<TransformComponent>();
                t->position = pos;
                t->scale = scale;
                if (withMesh)
                    e->AddComponent<MeshRendererComponent>()->color = color;
                return id;
            };

        make("Camera", { 0.0f, 1.0f, -5.0f }, { 1,1,1 }, false, {});
        make("Floor", { 0.0f,-0.5f,  0.0f }, { 6.0f, 0.2f, 6.0f }, true, { 0.35f,0.35f,0.38f,1 });
        make("Cube", { 0.0f, 0.5f,  0.0f }, { 1,1,1 }, true, { 0.85f,0.55f,0.25f,1 });
        make("Light", { 2.0f, 1.0f,  0.0f }, { 0.3f,0.3f,0.3f }, true, { 0.95f,0.9f,0.3f,1 });
        // GLB 表示のデモ（軽い .glb を指定）
        {
            EntityId id = world.CreateEntity("Crystal");
            Entity* e = world.Get(id);
            auto* t = e->AddComponent<TransformComponent>();
            t->position = { -2.0f, 0.5f, 0.0f };
            auto* m = e->AddComponent<MeshRendererComponent>();
            m->color = { 0.6f, 0.85f, 1.0f, 1.0f };
            m->assetId = "Data/Model/STAGE GLB format/detail-crystal.glb";
        }
        return true;
    }

    int Engine::Run()
    {
        while (window->ProcessMessages())
        {
            imgui->NewFrame();                                   // ① NewFrame
            DrawMainMenuBar(world, selected, history);           // ② メインメニュー（先に）
            imgui->BeginDockspace();                             // ③ DockSpace（メニュー下に配置）
            HandleShortcuts(history);
            viewport->DrawUI(graphics->GetDevice(), world, selected, history);
            DrawHierarchyPanel(world, selected, history);   
            DrawInspectorPanel(world, selected, history, inspectorSession, meshCache, graphics->GetDevice());
            DrawAssetBrowserPanel(world, selected, history);
            Update();

            viewport->RenderScene(graphics->GetContext(), world, meshCache, graphics->GetDevice());
            graphics->BeginFrame(0.10f, 0.12f, 0.15f, 1.0f);
            imgui->EndFrame();
            graphics->EndFrame();
        }
        return 0;
    }

    void Engine::Update() {}

} // namespace uel