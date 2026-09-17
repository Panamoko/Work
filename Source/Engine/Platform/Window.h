#pragma once
#include <Windows.h>
#include <functional>

namespace uel {

    class Window {
    public:
        // 上位がメッセージを先取りするためのフック。true を返すと「消費済み」。
        using MessageHook = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;

        Window(HINSTANCE instance, const wchar_t* title, int width, int height);
        ~Window();
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        bool ProcessMessages();
        void SetMessageHook(MessageHook hook) { messageHook = std::move(hook); }

        HWND GetHandle() const { return hwnd; }
        int  GetWidth()  const { return width; }
        int  GetHeight() const { return height; }

    private:
        static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
        LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);

        HWND           hwnd = nullptr;
        HINSTANCE      hInstance = nullptr;
        int            width = 0;
        int            height = 0;
        MessageHook    messageHook;
        const wchar_t* className = L"UELikeEngineWindow";
    };

} // namespace uel