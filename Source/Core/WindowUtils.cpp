#include "Core/WindowUtils.h"

namespace WindowUtils
{
	void ToggleFullscreen(HWND hWnd, bool& isFullscreen, RECT& savedWindowRect)
	{
		if (hWnd == nullptr) return;

		if (!isFullscreen) {
			GetWindowRect(hWnd, &savedWindowRect);
			SetWindowLong(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
			HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFO mi; mi.cbSize = sizeof(mi); GetMonitorInfo(hMonitor, &mi);
			SetWindowPos(hWnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			isFullscreen = true;
		}
		else {
			SetWindowLong(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
			SetWindowPos(hWnd, nullptr, savedWindowRect.left, savedWindowRect.top, savedWindowRect.right - savedWindowRect.left, savedWindowRect.bottom - savedWindowRect.top, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			isFullscreen = false;
		}
	}

	void UpdateCursorLock(HWND hWnd, bool locked, bool& cursorVisible,
		float screenWidth, float screenHeight,
		float& outDeltaX, float& outDeltaY)
	{
		outDeltaX = 0.0f;
		outDeltaY = 0.0f;

		if (hWnd != GetForegroundWindow()) return;

		if (locked) {
			if (cursorVisible) { ShowCursor(FALSE); cursorVisible = false; }
			POINT center = { static_cast<LONG>(screenWidth / 2.0f), static_cast<LONG>(screenHeight / 2.0f) };
			ClientToScreen(hWnd, &center);
			POINT currentMousePos; GetCursorPos(&currentMousePos);
			outDeltaX = static_cast<float>(currentMousePos.x - center.x);
			outDeltaY = static_cast<float>(currentMousePos.y - center.y);
			SetCursorPos(center.x, center.y);
		}
		else {
			if (!cursorVisible) { ShowCursor(TRUE); cursorVisible = true; }
		}
	}
}
