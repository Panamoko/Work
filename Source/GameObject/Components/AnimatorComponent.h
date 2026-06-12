#pragma once

#include <vector>
#include "GameObject/Component.h"
#include "GameObject/Components/ModelComponent.h"

class AnimatorComponent : public Component
{
public:
	AnimatorComponent() = default;
	~AnimatorComponent() override = default;

	const char* GetName() const override { return "Animator"; }

	void Start() override;
	void Update(float elapsedTime) override;
	void OnGUI() override;

	// 単一アニメーションの再生
	void Play(int index, float fadeLength = 0.15f, bool loop = true, float clampTime = -1.0f);

	// 2つのアニメーションのブレンド再生
	void PlayBlend(int indexA, int indexB, float blendRatio, float fadeLength = 0.15f, bool loop = true);

	// ブレンド割合の更新（移動中に毎フレーム呼ぶ）
	void UpdateBlendRatio(float blendRatio);

	// 再生速度の設定
	void SetAnimSpeed(float speed) { animSpeed = speed; }

	int GetCurrentAnimIndexA() const { return currentAnimIndexA; }
	int GetCurrentAnimIndexB() const { return currentAnimIndexB; }

private:
	void BlendNodePoses(std::vector<Model::NodePose>& outPoses, const std::vector<Model::NodePose>& posesA, const std::vector<Model::NodePose>& posesB, float weightB);

private:
	ModelComponent* cachedModelComponent = nullptr;

	std::vector<Model::NodePose> currentPoses;
	std::vector<Model::NodePose> previousPoses; // クロスフェードの起点ポーズ記憶用

	int currentAnimIndexA = -1;
	int currentAnimIndexB = -1;
	float currentBlendRatio = 0.0f;

	float animationSeconds = 0.0f;
	float currentAnimLength = 1.0f;
	float customClampTime = -1.0f;
	float animSpeed = 1.0f;
	bool isLoop = true;

	// クロスフェード管理用
	bool isFading = false;
	float fadeTimer = 0.0f;
	float fadeLength = 0.15f;
};