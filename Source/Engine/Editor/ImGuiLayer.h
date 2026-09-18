#pragma once
#include <Windows.h>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace uel {

    // Dear ImGui + DockSpace を隔離する Editor 層。
    class ImGuiLayer {
    public:
        void Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context);
        void Shutdown();

        void NewFrame();        // ImGui の NewFrame だけ
        void BeginDockspace();  // 画面全体の DockSpace ホスト
        void EndFrame();   // 描画データを現在のRTVへ出力

        // Window のメッセージフックに渡す静的ハンドラ（Window は ImGui を知らずに済む）。
        static bool HandleMessage(HWND, UINT, WPARAM, LPARAM);
    };

} // namespace uel