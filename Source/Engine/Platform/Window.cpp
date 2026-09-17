#include "Engine/Platform/Window.h"

namespace uel {

    Window::Window(HINSTANCE instance, const wchar_t* title, int width, int height)
        : hInstance(instance), width(width), height(height)
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInstance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.lpszClassName = className;
        RegisterClassExW(&wc);

        RECT rc = { 0, 0, width, height };
        DWORD style = WS_OVERLAPPEDWINDOW;
        AdjustWindowRect(&rc, style, FALSE);

        hwnd = CreateWindowExW(
            0, className, title, style | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top,
            nullptr, nullptr, hInstance, this); // 末尾 this でインスタンスに紐付け
    }

    Window::~Window()
    {
        if (hwnd) DestroyWindow(hwnd);
        UnregisterClassW(className, hInstance);
    }

    bool Window::ProcessMessages()
    {
        MSG msg = {};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) return false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        return true;
    }

    LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        if (msg == WM_NCCREATE)
        {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
            SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        }
        Window* self = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (self) return self->HandleMessage(hwnd, msg, wparam, lparam);
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

    LRESULT Window::HandleMessage(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
    {
        // まず ImGui 等のフックへ。消費されたら終了。
        if (messageHook && messageHook(hwnd, msg, wparam, lparam))
            return true;

        switch (msg)
        {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProc(hwnd, msg, wparam, lparam);
    }

} // namespace uel