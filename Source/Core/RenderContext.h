#pragma once

#include "Camera/Camera.h"
#include "Graphics/RenderState.h"
#include "Graphics/Light.h"

struct RenderContext
{
	// šŠù‘¶‚ÌŒ^iconst“™j‚ğâ‘Î‚É•ö‚³‚¸Aƒˆ‚É‰Šú‰»(= nullptr)‚¾‚¯‚ğ’Ç‰Á‚µ‚Ü‚µ‚½
	ID3D11DeviceContext* deviceContext = nullptr;
	const RenderState* renderState = nullptr;
	const Camera* camera = nullptr;
	const LightManager* lightManager = nullptr;
};