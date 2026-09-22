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

namespace uel {

    class Engine {
    public:
        ~Engine();
        bool Initialize(HINSTANCE instance, const wchar_t* title, int width, int height);
        int  Run();

    private:
        void Update();

        std::unique_ptr<Window>         window;
        std::unique_ptr<GraphicsDevice> graphics;
        std::unique_ptr<ImGuiLayer>     imgui;
        std::unique_ptr<ViewportPanel>  viewport;

        World                world;
        EntityId             selected;
        CommandHistory       history;
        TransformEditSession inspectorSession;   // Inspectorï“èWóp
        MeshCache            meshCache;
    };

} // namespace uel