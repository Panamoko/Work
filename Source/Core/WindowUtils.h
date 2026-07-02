#pragma once

#include <Windows.h>

// Win32 window / cursor helpers kept out of gameplay code.
namespace WindowUtils
{
	// Toggle borderless-fullscreen <-> windowed, remembering the windowed rect.
	// No-op when hWnd is null.
	void ToggleFullscreen(HWND hWnd, bool& isFullscreen, RECT& savedWindowRect);

	// When locked (and the window is focused), hides the cursor, recenters it,
	// and reports the pixel delta since the previous call. Reports zero delta
	// when unlocked or the window is not in the foreground.
	void UpdateCursorLock(HWND hWnd, bool locked, bool& cursorVisible,
		float screenWidth, float screenHeight,
		float& outDeltaX, float& outDeltaY);
}
