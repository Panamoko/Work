#pragma once

#include <windows.h>
#include <DirectXMath.h>
#include <cmath>
#include "Camera/Camera.h"
#include "Core/Input.h"

// Free-fly editor camera (UE-like): hold RMB to look, WASD to move,
// E/Q up-down, Shift to move faster. Movement only while RMB is held
// so typing in ImGui fields never moves the camera.
class EditorCamera
{
public:
	void SyncFromCamera(const Camera& camera)
	{
		eye = camera.GetEye();
		DirectX::XMFLOAT3 f = camera.GetFront();
		pitch = asinf(f.y);
		yaw = atan2f(f.x, f.z);
	}

	void Update(Camera& camera, float dt)
	{
		Mouse& mouse = Input::Instance().GetMouse();
		int mx = mouse.GetPositionX();
		int my = mouse.GetPositionY();
		bool look = (mouse.GetButton() & Mouse::BTN_RIGHT) != 0;

		if (look && initialized)
		{
			float dx = (float)(mx - prevX);
			float dy = (float)(my - prevY);
			yaw += dx * sensitivity;
			pitch -= dy * sensitivity;
			const float lim = 1.55f;
			if (pitch > lim) pitch = lim;
			if (pitch < -lim) pitch = -lim;
		}
		prevX = mx; prevY = my; initialized = true;

		DirectX::XMFLOAT3 front = {
			cosf(pitch) * sinf(yaw),
			sinf(pitch),
			cosf(pitch) * cosf(yaw)
		};
		const DirectX::XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };

		// right = normalize(cross(up, front))
		DirectX::XMFLOAT3 right = {
			up.y * front.z - up.z * front.y,
			up.z * front.x - up.x * front.z,
			up.x * front.y - up.y * front.x
		};
		float rl = sqrtf(right.x * right.x + right.y * right.y + right.z * right.z);
		if (rl > 1e-5f) { right.x /= rl; right.y /= rl; right.z /= rl; }

		if (look)
		{
			float spd = speed * dt;
			if (GetAsyncKeyState(VK_SHIFT) & 0x8000) spd *= fastMultiplier;

			if (GetAsyncKeyState('W') & 0x8000) { eye.x += front.x * spd; eye.y += front.y * spd; eye.z += front.z * spd; }
			if (GetAsyncKeyState('S') & 0x8000) { eye.x -= front.x * spd; eye.y -= front.y * spd; eye.z -= front.z * spd; }
			if (GetAsyncKeyState('D') & 0x8000) { eye.x += right.x * spd; eye.y += right.y * spd; eye.z += right.z * spd; }
			if (GetAsyncKeyState('A') & 0x8000) { eye.x -= right.x * spd; eye.y -= right.y * spd; eye.z -= right.z * spd; }
			if (GetAsyncKeyState('E') & 0x8000) { eye.y += spd; }
			if (GetAsyncKeyState('Q') & 0x8000) { eye.y -= spd; }
		}

		DirectX::XMFLOAT3 focus = { eye.x + front.x, eye.y + front.y, eye.z + front.z };
		camera.SetLookAt(eye, focus, up);
	}

	float speed = 8.0f;
	float fastMultiplier = 4.0f;
	float sensitivity = 0.004f;

private:
	DirectX::XMFLOAT3 eye = { 10.0f, 5.0f, 15.0f };
	float yaw = 0.0f, pitch = 0.0f;
	int prevX = 0, prevY = 0;
	bool initialized = false;
};