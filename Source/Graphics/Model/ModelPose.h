#pragma once

#include <memory>
#include <vector>
#include <DirectXMath.h>
#include "Graphics/Model/Model.h"

// Per-instance animated pose of a shared Model.
//
// The Model (meshes, GPU buffers, materials, animation clips, bind hierarchy)
// stays shared/immutable through the ResourceManager flyweight. Each ModelPose
// owns its own copy of the node transforms, so two actors that use the same
// model file never overwrite each other's position/animation.
class ModelPose
{
public:
	void SetModel(const std::shared_ptr<Model>& src);

	bool IsValid() const { return model != nullptr; }
	Model* GetModel() const { return model.get(); }
	const std::shared_ptr<Model>& GetModelPtr() const { return model; }

	// Sample an animation clip into this instance's nodes (does not advance
	// time). Nodes without keyframes keep their bind pose.
	void ComputeAnimation(int animationIndex, float time);

	// Apply an externally computed pose (e.g. from AnimatorComponent blending).
	void SetNodePoses(const std::vector<Model::NodePose>& poses);

	// Override one node's local scale (used for the FPS head hide).
	void SetNodeScale(int nodeIndex, const DirectX::XMFLOAT3& scale);

	// Recompute local / global / world transforms for every node.
	void UpdateTransform(const DirectX::XMFLOAT4X4& world);

	const std::vector<Model::Node>& GetNodes() const { return nodes; }

private:
	std::shared_ptr<Model>		model;
	std::vector<Model::Node>	nodes; // working copy: own transforms
};