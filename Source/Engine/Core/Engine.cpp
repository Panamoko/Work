#include "Engine/Core/Engine.h"

namespace uel {

    Engine::~Engine()
    {
        if (imgui) imgui->Shutdown(); // device/window がまだ生きている間に終了
    }

    bool Engine::Initialize(HINSTANCE instance, const wchar_t* title, int width, int height)
    {
        window = std::make_unique<Window>(instance, title, width, height);
        graphics = std::make_unique<GraphicsDevice>();
        if (!graphics->Initialize(window->GetHandle(), width, height))
            return false;

        imgui = std::make_unique<ImGuiLayer>();
        imgui->Initialize(window->GetHandle(), graphics->GetDevice(), graphics->GetContext());

        // Window のメッセージを ImGui へ転送（Window は ImGui 非依存のまま）。
        window->SetMessageHook(&ImGuiLayer::HandleMessage);
        return true;
    }

    int Engine::Run()
    {
        while (window->ProcessMessages())
        {
            imgui->BeginFrame();                          // UI 構築（NewFrame + DockSpace）
            Update();

            graphics->BeginFrame(0.10f, 0.12f, 0.15f, 1.0f); // RTV クリア
            Render();                                      // シーン描画（Step 以降）
            imgui->EndFrame();                             // ImGui を RTV へ描画

            graphics->EndFrame();                          // Present
        }
        return 0;
    }

    void Engine::Update() {}
    void Engine::Render() {}

} // namespace uel