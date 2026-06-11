#pragma once

#include <memory>
#include <vector>
#include "GameObject/Component.h"
#include "Graphics/Model/Model.h"
#include "Graphics/Model/ModelRenderer.h"

// 3Dモデルを描画・管理し、アニメーションも再生するコンポーネント
class ModelComponent : public Component
{
public:
	// コンストラクタでモデルを読み込む
	ModelComponent(const char* filename);
	~ModelComponent() override = default;

	const char* GetName() const override { return "ModelComponent"; }

	// 更新処理（アニメーション進行と行列の同期）
	void Update(float elapsedTime) override;

	// 描画登録処理
	void Draw() override;

	// インスペクタ（GUI）描画
	void OnGUI() override;

	// 外部からモデルを取得したい場合用
	std::shared_ptr<Model> GetModel() const { return model; }

	// シェーダーの変更
	void SetShaderId(ShaderId id) { shaderId = id; }

	// ===============================================
	// アニメーション制御
	// ===============================================
	// 名前を指定してアニメーションを再生する
	void PlayAnimation(const char* animName);

	void SetAnimationSpeed(float speed) { animSpeed = speed; }
	float GetAnimationSpeed() const { return animSpeed; }

private:
	std::shared_ptr<Model>	model;
	ShaderId				shaderId = ShaderId::Lambert;

	// アニメーション管理用変数
	std::vector<Model::NodePose>	nodePoses;
	int								animationIndex = -1;
	float							animationSeconds = 0.0f;
	float							animSpeed = 1.0f;
};