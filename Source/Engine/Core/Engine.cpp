#include "Engine/Core/Engine.h"
#include "Engine/Editor/EditorPanels.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/MeshRendererComponent.h"
#include "Engine/Game/PlayerComponent.h"
#include "Engine/Scene/HPComponent.h"
#include "Engine/Game/OpponentComponent.h"
#include "Engine/Serialization/StageSerializer.h"
#include "Engine/Assets/GltfSkinnedLoader.h"
#include "Engine/Game/ScreenUIComponent.h"

#include <filesystem>
#include <imgui.h>
#include <cwchar>

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
        baseTitle = title;

        graphics = std::make_unique<GraphicsDevice>();
        if (!graphics->Initialize(window->GetHandle(), width, height))
            return false;

        imgui = std::make_unique<ImGuiLayer>();
        imgui->Initialize(window->GetHandle(), graphics->GetDevice(), graphics->GetContext());
        window->SetMessageHook(&ImGuiLayer::HandleMessage);   // ← CreateContext の後に設定

        viewport = std::make_unique<ViewportPanel>();
        viewport->Initialize(graphics->GetDevice());

        // --- Scene一覧：Scenes.json があればそれを使う。無ければ初回用のデモ2Sceneを作る ---
        std::vector<std::string> savedScenes;
        if (StageSerializer::LoadSceneList(savedScenes, "Scenes.json") && !savedScenes.empty()) {
            for (const auto& n : savedScenes) scenes.push_back({ n, World{}, EntityId{} });
        }
        else {
            scenes.push_back({ "Stage",      World{}, EntityId{} });
            scenes.push_back({ "Characters", World{}, EntityId{} });

            auto makeIn = [](World& w, const char* name, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 scale,
                bool withMesh, DirectX::XMFLOAT4 color) {
                    EntityId id = w.CreateEntity(name);
                    Entity* e = w.Get(id);
                    auto* t = e->AddComponent<TransformComponent>(); t->position = pos; t->scale = scale;
                    if (withMesh) e->AddComponent<MeshRendererComponent>()->color = color;
                    return id;
                };

            // Stage シーン（ゲーム配置用）
            {
                World& w = scenes[0].world;
                makeIn(w, "Camera", { 0,1,-5 }, { 1,1,1 }, false, {});
                makeIn(w, "Floor", { 0,-0.5f,0 }, { 6,0.2f,6 }, true, { 0.35f,0.35f,0.38f,1 });
                EntityId cubeId = makeIn(w, "Cube", { 0,0.5f,0 }, { 1,1,1 }, true, { 0.85f,0.55f,0.25f,1 });
                makeIn(w, "Light", { 2,1,0 }, { 0.3f,0.3f,0.3f }, true, { 0.95f,0.9f,0.3f,1 });
                if (Entity* e = w.Get(cubeId)) e->AddComponent<HPComponent>();
                {
                    EntityId id = w.CreateEntity("Enemy"); Entity* e = w.Get(id);
                    e->AddComponent<TransformComponent>()->position = { 2,0,2 };
                    e->AddComponent<MeshRendererComponent>()->assetId = "Data/Model/GLB format/character-b.glb";
                    e->AddComponent<OpponentComponent>();
                    e->AddComponent<HPComponent>();
                }
            }

            // Characters シーン（Player/敵の仕様確認用）
            {
                World& w = scenes[1].world;
                makeIn(w, "Floor", { 0,-0.5f,0 }, { 8,0.2f,4 }, true, { 0.30f,0.30f,0.33f,1 });
                const char* files[] = {
                    "Data/Model/GLB format/character-a.glb",
                    "Data/Model/GLB format/character-b.glb",
                    "Data/Model/GLB format/character-c.glb",
                };
                for (int i = 0; i < 3; ++i) {
                    EntityId id = w.CreateEntity("Char"); Entity* e = w.Get(id);
                    e->AddComponent<TransformComponent>()->position = { (float)(i * 2 - 2), 0.0f, 0.0f };
                    e->AddComponent<MeshRendererComponent>()->assetId = files[i];
                    e->AddComponent<HPComponent>();
                }
            }
        }

        // 保存済みファイルがあれば読み込む（無ければデモ配置のまま）
        for (auto& s : scenes)
            StageSerializer::Load(s.world, (s.name + ".json").c_str());
            StageSerializer::LoadEnvironment(lighting, postSettings, "Environment.json");   // 無ければ既定値のまま
        {
            SkinnedModel sm;
            if (LoadSkinnedModel("Data/YourRiggedModel.glb", sm)) {
                char buf[256];
                sprintf_s(buf, "[Skinned] bones=%zu verts=%zu tris=%zu clips=%zu\n",
                    sm.skeleton.bones.size(), sm.vertices.size(), sm.indices.size() / 3, sm.clips.size());
                OutputDebugStringA(buf);
                for (auto& c : sm.clips) {
                    sprintf_s(buf, "  clip '%s' dur=%.2f tracks=%zu\n", c.name.c_str(), c.duration, c.tracks.size());
                    OutputDebugStringA(buf);
                }
            }
            else OutputDebugStringA("[Skinned] load failed\n");
        }
        return true;
    }

    int Engine::Run()
    {
        while (window->ProcessMessages())
        {
            graphics->Resize(window->GetWidth(), window->GetHeight());
            imgui->NewFrame();
            // タイトルバーにFPSを表示（0.5秒ごとに更新）
            titleTimer += ImGui::GetIO().DeltaTime;
            if (titleTimer >= 0.5f) {
                titleTimer = 0.0f;
                float fps = ImGui::GetIO().Framerate;
                wchar_t buf[256];
                swprintf_s(buf, L"%s  |  %.1f FPS  (%.2f ms)%s", baseTitle.c_str(), fps,
                    fps > 0.0f ? 1000.0f / fps : 0.0f,
                    mode == EditorMode::Play ? L"  [PLAY]" : L"");
                SetWindowTextW(window->GetHandle(), buf);
            }

            // Scene名リスト
            std::vector<std::string> names; names.reserve(scenes.size());
            for (auto& s : scenes) names.push_back(s.name);

            bool togglePlay = false, addScene = false, sceneSaved = false;
            std::string renameScene;
            DrawMainMenuBar(scenes[activeScene].world, scenes[activeScene].selected, history,
                mode == EditorMode::Play, togglePlay, names, activeScene, addScene, renameScene, sceneSaved);
            if (sceneSaved) StageSerializer::SaveEnvironment(lighting, postSettings, "Environment.json");
            if (addScene) {
                auto taken = [&](const std::string& nm) { for (auto& s : scenes) if (s.name == nm) return true; return false; };
                int n = (int)scenes.size();
                std::string nm;
                do { nm = "Scene" + std::to_string(n++); } while (taken(nm));
                scenes.push_back({ nm, World{}, EntityId{} });
            }
            if (togglePlay) { if (mode == EditorMode::Edit) StartPlay(); else StopPlay(); }
            // Play中のScene遷移（前フレームに出されたリクエストを処理）
            if (mode == EditorMode::Play) {
                GameFlow& flow = viewport->GetFlow();
                if (!flow.requestedScene.empty()) {
                    std::string req = flow.requestedScene;
                    flow.requestedScene.clear();
                    for (int i = 0; i < (int)scenes.size(); ++i)
                        if (scenes[i].name == req) { LoadPlayScene(i, false); break; }
                }
            }
            if (!renameScene.empty() && mode == EditorMode::Edit) RenameActiveScene(renameScene);
            // ここで確定（addScene後・activeScene確定後に束縛）
            EditorScene& sc = scenes[activeScene];
            World& world = sc.world;
            EntityId& selected = sc.selected;

            imgui->BeginDockspace();
            HandleShortcuts(history);

            World& aw = (mode == EditorMode::Play) ? playWorld : world;   // 描画/更新はアクティブ
            viewport->DrawUI(graphics->GetDevice(), aw, selected, history, mode == EditorMode::Play, meshCache);
            DrawHierarchyPanel(world, selected, history);                 // パネルは編集World
            DrawInspectorPanel(world, selected, history, inspectorSession, meshCache, graphics->GetDevice());
            DrawAssetBrowserPanel(world, selected, history);
            DrawEnvironmentPanel(lighting, postSettings);
            DrawAnimationEditorPanel(world, selected, graphics->GetDevice());
            DrawNavGridPanel(world);
            // Prefab が Apply されたら、全Sceneの同じPrefabのインスタンスへ反映
            std::string refreshed = ConsumePrefabRefreshRequest();
            if (!refreshed.empty())
                for (auto& s : scenes) StageSerializer::RefreshPrefabInstances(s.world, refreshed.c_str());
            Update();

            viewport->RenderScene(graphics->GetContext(), aw, meshCache, graphics->GetDevice(), lighting, postSettings);
            graphics->BeginFrame(0.10f, 0.12f, 0.15f, 1.0f);
            imgui->EndFrame();
            graphics->EndFrame();
        }
        return 0;
    }

    void Engine::Update() {}

    void Engine::StartPlay()
    {
        mode = EditorMode::Play;
        viewport->GetFlow().lastResult.clear();
        LoadPlayScene(activeScene, true);
    }

    void Engine::LoadPlayScene(int index, bool allowSelectedFallback)
    {
        EditorScene& sc = scenes[index];
        playWorld = sc.world.Clone();

        GameFlow& flow = viewport->GetFlow();
        flow.requestedScene.clear();
        flow.playTime = 0.0f;

        // プレイヤー = PlayerComponent を持つEntity
        Entity* player = nullptr;
        bool hasScreenUI = false;
        for (const auto& slot : playWorld.Slots()) {
            if (!slot) continue;
            if (!player && slot->GetComponent<PlayerComponent>()) player = slot.get();
            if (slot->GetComponent<ScreenUIComponent>()) hasScreenUI = true;
        }

        // 保険：最初のPlay開始時だけ、選択Entityをプレイヤーにする（Title/Result画面では行わない）
        if (!player && allowSelectedFallback && !hasScreenUI)
            if (Entity* e = playWorld.Get(sc.selected)) { e->AddComponent<PlayerComponent>(); player = e; }

        if (player) {
            if (auto* m = player->GetComponent<MeshRendererComponent>()) m->firstPerson = true;
            auto* php = player->GetComponent<HPComponent>();
            if (!php) php = player->AddComponent<HPComponent>();
            php->hp = php->maxHp;
        }
    }

    void Engine::StopPlay() { mode = EditorMode::Edit; }

    bool Engine::RenameActiveScene(const std::string& newName)
    {
        if (newName.empty() || newName.find_first_of("\\/:*?\"<>|") != std::string::npos) return false; // ファイル名に使えない文字
        for (auto& s : scenes) if (s.name == newName) return false;                                     // 同名は不可

        EditorScene& sc = scenes[activeScene];
        std::error_code ec;
        std::filesystem::path oldFile = sc.name + ".json";
        std::filesystem::path newFile = newName + ".json";
        if (std::filesystem::exists(oldFile, ec)) std::filesystem::rename(oldFile, newFile, ec);          // 保存ファイルも改名
        if (ec) return false;

        sc.name = newName;
        std::vector<std::string> names;
        for (auto& s : scenes) names.push_back(s.name);
        StageSerializer::SaveSceneList(names, "Scenes.json");                                            // 一覧も更新
        return true;
    }

} // namespace uel