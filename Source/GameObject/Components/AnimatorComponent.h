#pragma once

#include <vector>
#include <memory>
#include "GameObject/Component.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Definition/AnimationSet.h"

class AnimatorComponent : public Component
{
public:
	AnimatorComponent() = default;
	~AnimatorComponent() override = default;

	const char* GetName() const override { return "Animator"; }

	void Start() override;
	void Update(float elapsedTime) override;
	void OnGUI() override;

	// ★モデル差し替え後の再バインド（既存機能・落とさないこと）
	void RebindModel();

	// 単一アニメーションの再生（従来通り・index直指定）
	void Play(int index, float fadeLength = 0.15f, bool loop = true, float clampTime = -1.0f);

	// ★名前非依存の再生：LogicalAnimationRole から再生（実Clip名を知らなくてよい）
	void Play(LogicalAnimationRole role, float fadeOverride = -1.0f);

	// AnimationSet（Role->Clip対応表）を差し込む。モデルへ即解決する。
	void SetAnimationSet(std::shared_ptr<AnimationSet> set);
	const AnimationSet* GetAnimationSet() const { return animationSet.get(); }

	// 2つのアニメーションのブレンド再生
	void PlayBlend(int indexA, int indexB, float blendRatio, float fadeLength = 0.15f, bool loop = true);

	// ブレンド割合の更新（移動時に毎フレーム呼ぶ）
	void UpdateBlendRatio(float blendRatio);

	// 再生速度の設定
	void SetAnimSpeed(float speed) { animSpeed = speed; }

	int GetCurrentAnimIndexA() const { return currentAnimIndexA; }
	int GetCurrentAnimIndexB() const { return currentAnimIndexB; }
	float GetAnimationSeconds() const { return animationSeconds; }
	float GetAnimationLength() const { return currentAnimLength; }
	float GetNormalizedTime() const { return currentAnimLength > 0.0001f ? animationSeconds / currentAnimLength : 0.0f; }
	bool  IsAnimationFinished() const { return !isLoop && animationSeconds >= currentAnimLength - 0.01f; }

private:
	void BlendNodePoses(std::vector<Model::NodePose>& outPoses, const std::vector<Model::NodePose>& posesA, const std::vector<Model::NodePose>& posesB, float weightB);

private:
	ModelComponent* cachedModelComponent = nullptr;

	std::shared_ptr<AnimationSet> animationSet; // Role->Clip 対応表（未設定可）

	std::vector<Model::NodePose> currentPoses;
	std::vector<Model::NodePose> previousPoses;

	int currentAnimIndexA = -1;
	int currentAnimIndexB = -1;
	float currentBlendRatio = 0.0f;

	float animationSeconds = 0.0f;
	float currentAnimLength = 1.0f;
	float customClampTime = -1.0f;
	float animSpeed = 1.0f;
	bool isLoop = true;

	bool isFading = false;
	float fadeTimer = 0.0f;
	float fadeLength = 0.15f;
};