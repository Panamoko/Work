#include <algorithm>
#include <cmath>
#include <imgui.h>
#include "GameObject/Components/AnimatorComponent.h"

void AnimatorComponent::Start()
{
	cachedModelComponent = GetActor()->GetComponent<ModelComponent>().get();

	if (cachedModelComponent && cachedModelComponent->GetModel())
	{
		cachedModelComponent->GetModel()->GetNodePoses(currentPoses);
		previousPoses = currentPoses;
	}
}

void AnimatorComponent::Play(int index, float fadeLength, bool loop, float clampTime)
{
	this->customClampTime = clampTime;
	PlayBlend(index, -1, 0.0f, fadeLength, loop);
}

void AnimatorComponent::PlayBlend(int indexA, int indexB, float blendRatio, float fadeLength, bool loop)
{
	if (!cachedModelComponent || !cachedModelComponent->GetModel()) return;

	// ★プロの技術：フェードの準備（現在のポーズを過去として記憶し、タイマーをリセット）
	if (fadeLength > 0.0f && currentPoses.size() > 0)
	{
		previousPoses = currentPoses;
		isFading = true;
		fadeTimer = 0.0f;
		this->fadeLength = fadeLength;
	}
	else
	{
		isFading = false;
	}

	currentAnimIndexA = indexA;
	currentAnimIndexB = indexB;
	currentBlendRatio = blendRatio;
	this->isLoop = loop;
	animationSeconds = 0.0f;

	// アニメーションの長さを取得し、寸止め設定があれば適用する
	Model* model = cachedModelComponent->GetModel().get();
	if (indexA >= 0)
	{
		currentAnimLength = model->GetAnimations().at(indexA).secondsLength;
		if (customClampTime > 0.0f)
		{
			currentAnimLength = (std::min)(currentAnimLength, customClampTime);
		}
	}
}

void AnimatorComponent::UpdateBlendRatio(float blendRatio)
{
	currentBlendRatio = blendRatio;
}

void AnimatorComponent::Update(float elapsedTime)
{
	if (!cachedModelComponent) return;
	Model* model = cachedModelComponent->GetModel().get();
	if (!model) return;

	animationSeconds += elapsedTime * animSpeed;

	if (!isLoop)
	{
		if (animationSeconds >= currentAnimLength)
			animationSeconds = currentAnimLength - 0.001f;
	}
	else
	{
		if (currentAnimLength > 0.0001f) {
			while (animationSeconds > currentAnimLength) {
				animationSeconds -= currentAnimLength;
			}
		}
	}

	if (currentAnimIndexA >= 0 && currentAnimIndexB >= 0)
	{
		std::vector<Model::NodePose> posesA = currentPoses;
		std::vector<Model::NodePose> posesB = currentPoses;

		float progressA = 0.0f;
		if (currentAnimLength > 0.0001f) progressA = animationSeconds / currentAnimLength;

		float animLengthB = model->GetAnimations().at(currentAnimIndexB).secondsLength;
		float animationSecondsB = animLengthB * progressA;

		model->ComputeAnimation(currentAnimIndexA, animationSeconds, posesA);
		model->ComputeAnimation(currentAnimIndexB, animationSecondsB, posesB);

		BlendNodePoses(currentPoses, posesA, posesB, currentBlendRatio);
	}
	else if (currentAnimIndexA >= 0)
	{
		model->ComputeAnimation(currentAnimIndexA, animationSeconds, currentPoses);
	}

	if (isFading)
	{
		fadeTimer += elapsedTime;
		float fadeWeight = fadeTimer / fadeLength;
		if (fadeWeight >= 1.0f)
		{
			isFading = false;
			fadeWeight = 1.0f;
		}
		std::vector<Model::NodePose> tempPoses = currentPoses;
		BlendNodePoses(currentPoses, previousPoses, tempPoses, fadeWeight);
	}

	// Only push a pose when we actually have a clip; otherwise leave the
	// instance at its bind pose (avoids distortion right after a model swap).
	if (currentAnimIndexA >= 0)
	{
		cachedModelComponent->SubmitAnimatorPose(currentPoses);
	}
}

void AnimatorComponent::BlendNodePoses(std::vector<Model::NodePose>& outPoses, const std::vector<Model::NodePose>& posesA, const std::vector<Model::NodePose>& posesB, float weightB)
{
	if (weightB <= 0.001f) { outPoses = posesA; return; }
	if (weightB >= 0.999f) { outPoses = posesB; return; }

	for (size_t i = 0; i < outPoses.size(); ++i)
	{
		DirectX::XMVECTOR pA = DirectX::XMLoadFloat3(&posesA[i].position);
		DirectX::XMVECTOR pB = DirectX::XMLoadFloat3(&posesB[i].position);
		DirectX::XMStoreFloat3(&outPoses[i].position, DirectX::XMVectorLerp(pA, pB, weightB));

		DirectX::XMVECTOR rA = DirectX::XMLoadFloat4(&posesA[i].rotation);
		DirectX::XMVECTOR rB = DirectX::XMLoadFloat4(&posesB[i].rotation);
		DirectX::XMStoreFloat4(&outPoses[i].rotation, DirectX::XMQuaternionSlerp(rA, rB, weightB));

		DirectX::XMVECTOR sA = DirectX::XMLoadFloat3(&posesA[i].scale);
		DirectX::XMVECTOR sB = DirectX::XMLoadFloat3(&posesB[i].scale);
		DirectX::XMStoreFloat3(&outPoses[i].scale, DirectX::XMVectorLerp(sA, sB, weightB));
	}
}

void AnimatorComponent::OnGUI()
{
	ImGui::TextColored(ImVec4(0, 1, 1, 1), u8"▼ Animator 内部状態");
	ImGui::Text(u8"クロスフェード: %s (%.2f / %.2f)", isFading ? "TRUE" : "FALSE", fadeTimer, fadeLength);
}