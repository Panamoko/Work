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

		// AnimationSetがあればこのModelの実Clipへ解決しておく
		if (animationSet)
			animationSet->ResolveAgainst(cachedModelComponent->GetModel().get());
	}
}

void AnimatorComponent::RebindModel()
{
	// Called after the actor's model was swapped at runtime. Re-cache the model
	// component (same component, new model) and rebuild the pose buffers for the
	// NEW skeleton. Clearing the current clip / crossfade prevents both an
	// out-of-range animation index and blending across two different skeletons.
	cachedModelComponent = GetActor()->GetComponent<ModelComponent>().get();

	currentAnimIndexA = -1;
	currentAnimIndexB = -1;
	currentBlendRatio = 0.0f;
	animationSeconds = 0.0f;
	currentAnimLength = 1.0f;
	isFading = false;
	fadeTimer = 0.0f;

	currentPoses.clear();
	previousPoses.clear();

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

	ImGui::Separator();
	ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), u8"▼ AnimationSet (Role -> Clip)");

	Model* model = (cachedModelComponent && cachedModelComponent->GetModel())
		? cachedModelComponent->GetModel().get() : nullptr;

	if (!model)
	{
		ImGui::TextDisabled(u8"Model 未リンク");
		return;
	}

	if (!animationSet)
	{
		ImGui::TextDisabled(u8"AnimationSet 未生成");
		if (ImGui::Button(u8"【 推定AnimationSetを生成 (MakeGuess) 】"))
		{
			animationSet = std::make_shared<AnimationSet>(AnimationSet::MakeGuess(model));
		}
		ImGui::TextDisabled(u8"※候補名は「例」。生成後にRole->Clipの解決結果が出ます");
		return;
	}

	// 生成済み：Role ごとの解決結果と、その場再生ボタン
	if (ImGui::Button(u8"再解決 (ResolveAgainst)")) animationSet->ResolveAgainst(model);
	ImGui::SameLine();
	if (ImGui::Button(u8"破棄")) { animationSet.reset(); return; }

	ImGui::Separator();
	for (int i = 0; i < static_cast<int>(LogicalAnimationRole::_Count); ++i)
	{
		LogicalAnimationRole role = static_cast<LogicalAnimationRole>(i);
		const AnimationRoleEntry* e = animationSet->Find(role);
		int idx = animationSet->ResolveIndex(role);

		ImGui::PushID(i);
		if (ImGui::SmallButton(u8"▶")) Play(role);   // ★Play(role)の動作確認
		ImGui::SameLine();
		if (e && !e->clipName.empty())
			ImGui::Text(u8"%-15s -> [%d] %s", ToString(role), idx, e->clipName.c_str());
		else if (idx >= 0)
			ImGui::TextDisabled(u8"%-15s -> fallback [%d]", ToString(role), idx);
		else
			ImGui::TextDisabled(u8"%-15s -> (未割当/未解決)", ToString(role));
		ImGui::PopID();
	}
}

void AnimatorComponent::SetAnimationSet(std::shared_ptr<AnimationSet> set)
{
	animationSet = std::move(set);
	if (animationSet && cachedModelComponent && cachedModelComponent->GetModel())
		animationSet->ResolveAgainst(cachedModelComponent->GetModel().get());
}

void AnimatorComponent::Play(LogicalAnimationRole role, float fadeOverride)
{
	if (!cachedModelComponent || !cachedModelComponent->GetModel()) return;

	// Mapping未設定なら推定Setをフォールバック生成（実名はコードに固定しない）
	if (!animationSet)
	{
		animationSet = std::make_shared<AnimationSet>(
			AnimationSet::MakeGuess(cachedModelComponent->GetModel().get()));
		OutputDebugStringA("[AnimationSet] no set assigned -> using MakeGuess fallback\n");
	}

	const AnimationRoleEntry* e = animationSet->Find(role);
	int index = animationSet->ResolveIndex(role);
	if (index < 0)
	{
		// 未割当/未解決：空再生せず Idle へフォールバック（それも無ければ何もしない）
		index = animationSet->ResolveIndex(LogicalAnimationRole::Idle);
		if (index < 0)
		{
			OutputDebugStringA("[AnimationSet] unresolved role and no Idle fallback -> skip\n");
			return;
		}
	}

	float fade = (fadeOverride >= 0.0f) ? fadeOverride : (e ? e->blendTime : 0.15f);
	bool  loop = e ? e->loop : true;
	SetAnimSpeed(e ? e->playbackRate : 1.0f);
	Play(index, fade, loop);   // 従来のindex再生へ委譲
}