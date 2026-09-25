#pragma once
#include <Windows.h>
#include <memory>
#include "Engine/Platform/Window.h"
#include "Engine/Graphics/GraphicsDevice.h"
#include "Engine/Editor/ImGuiLayer.h"
#include "Engine/Editor/ViewportPanel.h"
#include "Engine/Editor/CommandHistory.h"
#include "Engine/Editor/TransformEdit.h"
#include "Engine/Scene/World.h"
#include "Engine/Assets/MeshCache.h"
#include "Engine/Graphics/SceneLighting.h"

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

        std::unique_ptr<Window>         window;
        std::unique_ptr<GraphicsDevice> graphics;
        std::unique_ptr<ImGuiLayer>     imgui;
        std::unique_ptr<ViewportPanel>  viewport;

        World                world;
        EntityId             selected;
        CommandHistory       history;
        TransformEditSession inspectorSession;   // Inspector編集用
        MeshCache            meshCache;
        SceneLighting        lighting;

        EditorMode mode = EditorMode::Edit;
        EntityId   playViewEntity;
        bool       playPrevFirstPerson = false;
        bool           playHadTransform = false;   // ★
        TransformSnapshot playXform;               // ★開始時の位置を保存して復元

    };

} // namespace uel