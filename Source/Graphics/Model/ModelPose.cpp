#include "Graphics/Model/ModelPose.h"

void ModelPose::SetModel(const std::shared_ptr<Model>& src)
{
	model = src;
	nodes.clear();
	if (!model) return;

	nodes = model->GetNodes();

	// Re-point parent / children into THIS instance's vector.
	for (Model::Node& n : nodes)
	{
		n.children.clear();
		n.parent = (n.parentIndex >= 0) ? &nodes[n.parentIndex] : nullptr;
	}
	for (Model::Node& n : nodes)
	{
		if (n.parent) n.parent->children.emplace_back(&n);
	}

	DirectX::XMFLOAT4X4 identity;
	DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
	UpdateTransform(identity);
}

void ModelPose::ComputeAnimation(int animationIndex, float time)
{
	if (!model || animationIndex < 0) return;

	// Seed with the bind pose so nodes without keyframes are not reset.
	const std::vector<Model::Node>& bind = model->GetNodes();
	std::vector<Model::NodePose> poses(nodes.size());
	for (size_t i = 0; i < nodes.size(); ++i)
	{
		poses[i].position = bind[i].position;
		poses[i].rotation = bind[i].rotation;
		poses[i].scale = bind[i].scale;
	}

	model->ComputeAnimation(animationIndex, time, poses);
	SetNodePoses(poses);
}

void ModelPose::SetNodePoses(const std::vector<Model::NodePose>& poses)
{
	size_t n = (poses.size() < nodes.size()) ? poses.size() : nodes.size();
	for (size_t i = 0; i < n; ++i)
	{
		nodes[i].position = poses[i].position;
		nodes[i].rotation = poses[i].rotation;
		nodes[i].scale = poses[i].scale;
	}
}

void ModelPose::SetNodeScale(int nodeIndex, const DirectX::XMFLOAT3& scale)
{
	if (nodeIndex >= 0 && nodeIndex < static_cast<int>(nodes.size()))
	{
		nodes[nodeIndex].scale = scale;
	}
}

void ModelPose::UpdateTransform(const DirectX::XMFLOAT4X4& world)
{
	// Same algorithm as Model::UpdateTransform, on this instance's nodes.
	DirectX::XMMATRIX ParentWorldTransform = DirectX::XMLoadFloat4x4(&world);

	for (Model::Node& node : nodes)
	{
		DirectX::XMMATRIX S = DirectX::XMMatrixScaling(node.scale.x, node.scale.y, node.scale.z);
		DirectX::XMMATRIX R = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&node.rotation));
		DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(node.position.x, node.position.y, node.position.z);
		DirectX::XMMATRIX LocalTransform = S * R * T;

		DirectX::XMMATRIX ParentGlobalTransform =
			(node.parent != nullptr)
			? DirectX::XMLoadFloat4x4(&node.parent->globalTransform)
			: DirectX::XMMatrixIdentity();

		DirectX::XMMATRIX GlobalTransform = LocalTransform * ParentGlobalTransform;
		DirectX::XMMATRIX WorldTransform = GlobalTransform * ParentWorldTransform;

		DirectX::XMStoreFloat4x4(&node.localTransform, LocalTransform);
		DirectX::XMStoreFloat4x4(&node.globalTransform, GlobalTransform);
		DirectX::XMStoreFloat4x4(&node.worldTransform, WorldTransform);
	}
}