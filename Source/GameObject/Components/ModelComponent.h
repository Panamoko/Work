#pragma once

#include <memory>
#include <vector>
#include <string> 
#include "GameObject/Component.h"
#include "Graphics/Model/Model.h"
#include "Graphics/Model/ModelRenderer.h"

// 3Dモデルを描画・管理し、アニメーションも再生するコンポーネント
class ModelComponent : public Component
{
public:
	ModelComponent(const char* filename);
	~ModelComponent() override = default;

	const char* GetName() const override { return "ModelComponent"; }

	void Update(float elapsedTime) override;
	void Draw() override;
	void OnGUI() override;

	std::shared_ptr<Model> GetModel() const { return model; }
	void SetShaderId(ShaderId id) { shaderId = id; }

	void PlayAnimation(const char* animName);
	void SetAnimationSpeed(float speed) { animSpeed = speed; }
	float GetAnimationSpeed() const { return animSpeed; }

	void SetVisible(bool visible) { isVisible = visible; }
	bool IsVisible() const { return isVisible; }

	void SaveMaterialSettings();
	void LoadMaterialSettings();

private:
	std::shared_ptr<Model>	model;
	std::string             modelFilename;

	// =======================================================
	// ★プロの修正：デフォルトのシェーダーを Lambert から Toon に変更
	// =======================================================
	ShaderId				shaderId = ShaderId::Toon;

	std::vector<Model::NodePose>	nodePoses;
	int								animationIndex = -1;
	float							animationSeconds = 0.0f;
	float							animSpeed = 1.0f;

	bool							isVisible = true;
};