#pragma once

#include <memory>
#include <vector>
#include <string>
#include "GameObject/Component.h"
#include "Graphics/Model/Model.h"
#include "Graphics/Model/ModelRenderer.h"

// 3D model component: binds a .glb/.gltf model to an actor and manages
// drawing and animation.
class ModelComponent : public Component
{
public:
	ModelComponent(const char* filename);
	~ModelComponent() override = default;

	const char* GetName() const override { return "ModelComponent"; }

	void Update(float elapsedTime) override;
	void Draw() override;
	void OnGUI() override;
	void OnEditorSync() override; // push owner transform to the model in Edit mode

	std::shared_ptr<Model> GetModel() const { return model; }
	const std::string& GetModelPath() const { return modelFilename; }
	void SetModel(const char* filename);          // reload with a different model
	void SetShaderId(ShaderId id) { shaderId = id; }
	void SetVisible(bool visible) { isVisible = visible; }
	bool IsVisible() const { return isVisible; }

	void PlayAnimation(const char* animName);
	void SetAnimationSpeed(float speed) { animSpeed = speed; }
	float GetAnimationSpeed() const { return animSpeed; }

	void SaveMaterialSettings();
	void LoadMaterialSettings();

private:
	std::shared_ptr<Model>			model;
	std::string						modelFilename;
	ShaderId						shaderId = ShaderId::Toon;

	std::vector<Model::NodePose>	nodePoses;
	int								animationIndex = -1;
	float							animationSeconds = 0.0f;
	float							animSpeed = 1.0f;

	bool							isVisible = true;
};