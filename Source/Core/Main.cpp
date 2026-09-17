#include <windows.h>
#include <crtdbg.h>

#include "Engine/Core/Engine.h"

INT WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, INT)
{
#if defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    uel::Engine engine;
    if (!engine.Initialize(instance, L"UE-Like Engine (Rebuild)", 1280, 720))
        return -1;

    return engine.Run();
}