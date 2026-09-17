#pragma once
#include <Windows.h>
#include <memory>
#include "Engine/Platform/Window.h"
#include "Engine/Graphics/GraphicsDevice.h"
#include "Engine/Editor/ImGuiLayer.h"

namespace uel {

    class Engine {
    public:
        ~Engine();
        bool Initialize(HINSTANCE instance, const wchar_t* title, int width, int height);
        int  Run();

    private:
        void Update();
        void Render();

        std::unique_ptr<Window>         window;
        std::unique_ptr<GraphicsDevice> graphics;
        std::unique_ptr<ImGuiLayer>     imgui;
    };

} // namespace uel