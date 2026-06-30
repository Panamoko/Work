#pragma once

#include <memory>
#include <vector>
#include <string> 
#include "GameObject/Component.h"
#include "Graphics/Model/Model.h"
#include "Graphics/Model/ModelRenderer.h"

// ---------------------------------------------------------
// 3Dモデルコンポーネント
// アクターに3Dモデル(.glb/.gltf)を紐づけ、描画とアニメーションを管理する
// ---------------------------------------------------------
class ModelComponent : public Component
{
public:
	ModelComponent(const char* filename);
	~ModelComponent() override = default;

	const char* GetName() const override { return "ModelComponent"; }

	void Update(float elapsedTime) override;
	void Draw() override;
	void OnGUI() override;

	// ---- Getter / Setter ----
	std::shared_ptr<Model> GetModel() const { return model; }
	void SetShaderId(ShaderId id) { shaderId = id; }
	void SetVisible(bool visible) { isVisible = visible; }
	bool IsVisible() const { return isVisible; }

	// ---- アニメーション制御 ----
	void PlayAnimation(const char* animName);
	void SetAnimationSpeed(float speed) { animSpeed = speed; }
	float GetAnimationSpeed() const { return animSpeed; }

	// ---- マテリアル永続化 ----
	void SaveMaterialSettings();
	void LoadMaterialSettings();

private:
	// モデルデータ管理
	std::shared_ptr<Model>			model;
	std::string						modelFilename;
	ShaderId						shaderId = ShaderId::Toon; // デフォルトはトゥーン描画

	// アニメーション状態
	std::vector<Model::NodePose>	nodePoses;
	int								animationIndex = -1;
	float							animationSeconds = 0.0f;
	float							animSpeed = 1.0f;

	// 描画フラグ（FPS腕モデルの一時非表示などに使用）
	bool							isVisible = true;
};