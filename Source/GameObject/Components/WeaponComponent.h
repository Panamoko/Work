#pragma once

#include "GameObject/Component.h"
#include <DirectXMath.h>
#include <string>

// ★プロのC++技術：前方宣言でコンパイルを高速化
class Model;

class WeaponComponent : public Component
{
public:
	WeaponComponent() = default;
	~WeaponComponent() override = default;

	const char* GetName() const override { return "Weapon"; }

	void Start() override; // cpp側でLoadSettingsを呼ぶために宣言
	void Update(float elapsedTime) override {}
	void OnGUI() override;

	void SetWeaponTransform(const DirectX::XMMATRIX& parentMatrix);

	const char* GetTargetBoneName() const { return targetBoneName; }
	void SetTargetBoneName(const char* name) { strcpy_s(targetBoneName, sizeof(targetBoneName), name); }
	void SetTargetModel(const Model* model) { targetModel = model; }

	// ★Player側からの検索結果受け取り用
	void SetConnectedBoneIndex(int index) { currentConnectedBoneIndex = index; }

	// セーブ＆ロード関数
	void SaveSettings();
	void LoadSettings();

private:
	char								targetBoneName[64] = "mixamorig:RightHand";
	const Model* targetModel = nullptr;
	int									currentConnectedBoneIndex = -1; // 接続ステータス

	// 武器の初期オフセット
	DirectX::XMFLOAT3                   offsetPos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3                   offsetRot = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3                   offsetScale = { 10.0f, 10.0f, 10.0f };
};