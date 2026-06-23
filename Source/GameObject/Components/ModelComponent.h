#pragma once

#include <memory>
#include <vector>
#include <string> // ★追加：ファイル名保存用
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

	// アニメーション制御
	void PlayAnimation(const char* animName);

	void SetAnimationSpeed(float speed) { animSpeed = speed; }
	float GetAnimationSpeed() const { return animSpeed; }

	// 描画のON/OFF
	void SetVisible(bool visible) { isVisible = visible; }
	bool IsVisible() const { return isVisible; }

	// ===============================================
	// ★新設：マテリアルのセーブ＆ロード機能
	// ===============================================
	void SaveMaterialSettings();
	void LoadMaterialSettings();

private:
	std::shared_ptr<Model>	model;
	std::string             modelFilename; // ★追加：読み込んだモデルのパスを記憶
	ShaderId				shaderId = ShaderId::Lambert;

	// アニメーション管理用変数
	std::vector<Model::NodePose>	nodePoses;
	int								animationIndex = -1;
	float							animationSeconds = 0.0f;
	float							animSpeed = 1.0f;

	bool							isVisible = true;
};