#pragma once
#include <Windows.h>
#include <memory>
#include <vector>
#include <string>

#include "Engine/Platform/Window.h"
#include "Engine/Graphics/GraphicsDevice.h"
#include "Engine/Editor/ImGuiLayer.h"
#include "Engine/Editor/ViewportPanel.h"
#include "Engine/Editor/CommandHistory.h"
#include "Engine/Editor/TransformEdit.h"
#include "Engine/Scene/World.h"
#include "Engine/Graphics/SceneLighting.h"
#include "Engine/Assets/MeshCache.h"

namespace uel {

    enum class EditorMode { Edit, Play };

    class Engine {
    public:
        ~Engine();
        bool Initialize(HINSTANCE instance, const wchar_t* title, int width, int height);
        int  Run();

    private:
        void Update();
        void StartPlay();
        void StopPlay();
        bool RenameActiveScene(const std::string& newName);
        void LoadPlayScene(int index, bool allowSelectedFallback);

        std::unique_ptr<Window>         window;
        std::unique_ptr<GraphicsDevice> graphics;
        std::unique_ptr<ImGuiLayer>     imgui;
        std::unique_ptr<ViewportPanel>  viewport;

        struct EditorScene {
            std::string name;
            World       world;
            EntityId    selected;
        };
        std::vector<EditorScene> scenes;
        int   activeScene = 0;
        World playWorld;

        CommandHistory       history;
        TransformEditSession inspectorSession;
        MeshCache            meshCache;
        EditorMode           mode = EditorMode::Edit;
        SceneLighting        lighting;
        std::wstring         baseTitle;          // 元のウィンドウタイトル
        float                titleTimer = 0.0f;  // タイトル更新用タイマー
    };

} // namespace uel