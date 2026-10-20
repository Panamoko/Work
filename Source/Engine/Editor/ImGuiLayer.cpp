#include "Engine/Editor/ImGuiLayer.h"
#include <Windows.h>
#include <d3d11.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <filesystem>
#include <string>

// imgui_impl_win32.h が宣言する Win32 メッセージハンドラ。
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace uel {

    namespace {
        // 画面の文字の大きさ（ピクセル）。ImGui の標準フォントは 13
        constexpr float kUIFontSize = 18.0f;

        // 日本語を表示できるフォントを読み込む。上から順に探して、最初に見つかったものを使う。
        // どれも無ければ ImGui の標準フォント（英数字だけ）のまま。
        void LoadUIFont(ImGuiIO& io)
        {
            // 使う文字の範囲：日本語（ひらがな・カタカナ・よく使う漢字）＋ 記号
            // ImGui はフォントを作り終えるまでこの配列を見に行くので、static で残しておく
            static ImVector<ImWchar> ranges;
            if (ranges.empty()) {
                static const ImWchar extra[] = {
                    0x2010, 0x205E,     // ‐ … ※ などの記号
                    0x2190, 0x21FF,     // 矢印
                    0x2500, 0x257F,     // 罫線
                    0x25A0, 0x25FF,     // ■ ● ▲ ▶ など
                    0x2600, 0x26FF,     // ★ ☆ など
                    0,
                };
                ImFontGlyphRangesBuilder builder;
                builder.AddRanges(io.Fonts->GetGlyphRangesJapanese());
                builder.AddRanges(extra);
                builder.BuildRanges(&ranges);
            }

            char winDir[MAX_PATH] = "C:\\Windows";
            GetWindowsDirectoryA(winDir, MAX_PATH);
            const std::string fonts = std::string(winDir) + "\\Fonts\\";

            struct Candidate { std::string path; int faceIndex; };   // faceIndex … .ttc の中の何番目の書体か
            const Candidate candidates[] = {
                { fonts + "BIZ-UDGothicR.ttc", 1 },     // BIZ UDPゴシック（小さくても読みやすい）
                { fonts + "meiryo.ttc",        2 },     // Meiryo UI
                { fonts + "msgothic.ttc",      0 },     // MS ゴシック
                { "Data/Font/ArialUni.ttf",    0 },     // プロジェクトに入っているフォント
            };
            for (const Candidate& c : candidates) {
                std::error_code ec;
                if (!std::filesystem::exists(c.path, ec)) continue;    // 無いファイルを渡すと ImGui が止まる（assert）ので先に確かめる
                ImFontConfig cfg;
                cfg.FontNo = c.faceIndex;
                cfg.OversampleH = 1;        // 文字数が多いので、拡大して作らない（テクスチャを小さく）
                cfg.OversampleV = 1;
                cfg.PixelSnapH = true;      // 画素の境目にそろえて、くっきりさせる
                if (io.Fonts->AddFontFromFileTTF(c.path.c_str(), kUIFontSize, &cfg, ranges.Data)) return;
            }
        }
    }

    void ImGuiLayer::Initialize(HWND hwnd, ID3D11Device* device, ID3D11DeviceContext* context)
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // ドッキング有効
        io.IniFilename = "imgui.ini";                     // レイアウト保存先

        ImGui::StyleColorsDark(); // 8.2：濃色テーマ
        LoadUIFont(io);           // 日本語が出せる、大きめのフォント

        ImGui_ImplWin32_Init(hwnd);
        ImGui_ImplDX11_Init(device, context);
    }

    void ImGuiLayer::Shutdown()
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiLayer::NewFrame()
    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::BeginDockspace()
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->GetWorkPos());     // メインメニューバー分を除いた領域
        ImGui::SetNextWindowSize(vp->GetWorkSize());
        ImGui::SetNextWindowViewport(vp->ID);

        ImGuiWindowFlags hostFlags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("DockSpaceHost", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        ImGuiID dockId = ImGui::GetID("EngineDockSpace");
        ImGui::DockSpace(dockId, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
        ImGui::End();
    }

    void ImGuiLayer::EndFrame()
    {
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    bool ImGuiLayer::HandleMessage(HWND h, UINT m, WPARAM w, LPARAM l)
    {
        // コンテキスト未生成/破棄後にWin32メッセージが来ても安全に無視する。
        if (ImGui::GetCurrentContext() == nullptr) return false;   // ★追加
        return ImGui_ImplWin32_WndProcHandler(h, m, w, l) != 0;
    }

} // namespace uel
