#pragma once

#include <memory>
#include <DirectXMath.h>
#include "GameObject/Component.h"

class ModelComponent;
class AnimatorComponent;

// ---------------------------------------------------------
// 敵コンポーネント（AI＋被弾＋死亡）
//   ・索敵 → 向く → 追跡 → 停止
//   ・プレイヤーの射撃レイで被弾 → HP0 で死亡
// ---------------------------------------------------------
class EnemyComponent : public Component
{
public:
	EnemyComponent(std::shared_ptr<Actor> player);
	~EnemyComponent() override;

	const char* GetName() const override { return "Enemy"; }

	void Start() override;
	void Update(float elapsedTime) override;
	void OnGUI() override;

	// 被弾（外部＝射撃側から呼ぶ）
	void ApplyDamage(float dmg);

	// 全敵に対するレイキャスト（最も近い命中を返す。無ければ nullptr）
	static EnemyComponent* Raycast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDist, float& outDist);

	void SaveSettings();
	void LoadSettings();

private:
	enum class State { Idle, Walk };

	void PlayState(State s, bool force);
	void Die();
	bool RayHitSelf(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDist, float& outT);
	void Register();
	void Unregister();
	static float QuatToYaw(const DirectX::XMFLOAT4& q);

	// ---- 参照 ----
	std::weak_ptr<Actor> playerRef;
	ModelComponent* modelComp = nullptr;
	AnimatorComponent* animator = nullptr;

	// ---- アニメ index ----
	int idleIndex = -1;
	int walkIndex = -1;
	int fallIndex = -1;

	// ---- 状態 ----
	State state = State::Idle;
	float currentYaw = 0.0f;

	bool  dead = false;
	float deathTimer = 0.0f;
	float deathTime = 2.0f; // 死亡してから消えるまで(秒)

	// ---- ステータス ----
	float hp = 100.0f, maxHp = 100.0f;

	// ---- 当たり判定（球） ----
	float sphereCenterY = 1.0f; // 足元から中心までの高さ
	float sphereRadius = 1.0f; // 半径

	// ---- AIパラメータ ----
	float detectRange = 20.0f;
	float stopDistance = 2.0f;
	float moveSpeed = 2.5f;
	float turnSpeed = 8.0f;
	float blendTime = 0.2f;
};