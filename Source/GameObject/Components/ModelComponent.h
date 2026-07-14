#pragma once

#include <memory>
#include <vector>
#include <string>
#include "GameObject/Component.h"
#include "Graphics/Model/Model.h"
#include "Graphics/Model/ModelPose.h"
#include "Graphics/Model/ModelRenderer.h"

class ModelComponent : public Component
{
public:
	ModelComponent(const char* filename);
	~ModelComponent() override = default;

	const char* GetName() const override { return "ModelComponent"; }

	void Update(float elapsedTime) override;
	void Draw() override;
	void OnGUI() override;
	void OnEditorSync() override;

	std::shared_ptr<Model> GetModel() const { return model; }
	const std::string& GetModelPath() const { return modelFilename; }
	const std::vector<Model::Node>& GetPoseNodes() const { return pose.GetNodes(); }

	void SetModel(const char* filename);
	void SetShaderId(ShaderId id) { shaderId = id; }
	void SetVisible(bool visible) { isVisible = visible; }
	bool IsVisible() const { return isVisible; }

	void SetHideHead(bool hide) { hideHead = hide; }
	bool IsHeadHidden() const { return hideHead; }

	// Directly set this instance's world transform (e.g. a bone-attached weapon).
	// Call after the AnimatorComponent has submitted its pose this frame.
	void ApplyWorldTransform(const DirectX::XMFLOAT4X4& world) { if (pose.IsValid()) pose.UpdateTransform(world); }

	void SubmitAnimatorPose(const std::vector<Model::NodePose>& poses);

	void PlayAnimation(const char* animName);
	void SetAnimationSpeed(float speed) { animSpeed = speed; }
	float GetAnimationSpeed() const { return animSpeed; }

	void SaveMaterialSettings();
	void LoadMaterialSettings();

private:
	void CacheHeadNode();

	std::shared_ptr<Model>	model;
	ModelPose				pose;
	std::string				modelFilename;
	ShaderId				shaderId = ShaderId::Toon;

	int		animationIndex = -1;
	float	animationSeconds = 0.0f;
	float	animSpeed = 1.0f;
	bool	animatorDrove = false;

	bool	isVisible = true;

	int					headNodeIndex = -1;
	DirectX::XMFLOAT3	headBaseScale = { 1.0f, 1.0f, 1.0f };
	bool				hideHead = false;

	char	pathBuf[256] = { 0 };
};