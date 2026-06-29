#pragma once

#include <DirectXMath.h>

struct DirectionalLight
{
	DirectX::XMFLOAT3	direction = { -0.4f, -1.0f, -0.3f }; // diagonal sun (so shadows are visible)
	DirectX::XMFLOAT3	color = { 1, 1, 1 };
};

class LightManager
{
public:
	void SetDirectionalLight(const DirectionalLight& light) { directionalLight = light; }
	const DirectionalLight& GetDirectionalLight() const { return directionalLight; }

private:
	DirectionalLight	directionalLight;
};