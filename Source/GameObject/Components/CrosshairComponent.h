#pragma once

#include "GameObject/Component.h"

// 画面中央にValorant風のクロスヘアを描画するコンポーネント
class CrosshairComponent : public Component
{
public:
	CrosshairComponent() = default;
	~CrosshairComponent() override = default;

	const char* GetName() const override { return "Crosshair"; }

	void Start() override;
	void Update(float elapsedTime) override;
	void OnGUI() override;

	void SaveSettings();
	void LoadSettings();

	// ★外部（ViewModelなど）から呼び出して、射撃時にクロスヘアを開かせる関数
	void AddDynamicOffset(float offset) { dynamicOffset += offset; }

private:
	bool isEnabled = true;

	// クロスヘアのパラメータ群
	bool showCenterDot = false;
	float centerDotSize = 4.0f;
	float innerLineThickness = 2.0f;
	float innerLineLength = 6.0f;
	float innerLineOffset = 3.0f;
	float crosshairColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f }; // デフォルトは緑

	// アニメーション（反動）用パラメータ
	float dynamicOffset = 0.0f;       // 射撃時などに一時的に広がる値
	float offsetRecoverySpeed = 20.0f; // 広がったクロスヘアが元に戻るスピード
};