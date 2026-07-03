#pragma once

#include <memory>
#include <DirectXMath.h>
#include "AI/BehaviorTree.h" 
#include "GameObject/Component.h"
#include "Physics/CollisionMesh.h"

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

	// ---- AI action API（ビヘイビアツリーの末端ノードが呼ぶ） ----
	void  SetWanderTarget();               // homeを中心にランダムな徘徊目標を設定
	bool  MoveToTarget(float elapsedTime); // targetへ移動。到着したら true
	void  PlayWalk();
	void  PlayIdle();
	float NextIdleDuration();              // 待機時間（ランダム）

	// ---- Perception / chase (Step 3) ----
	void SetStageCollision(const CollisionMesh& mesh); // 視線判定用のステージ当たりを設定
	bool CanSeePlayer();                               // 距離＋視野角＋視線(壁)で判定
	void ChaseUpdate(float elapsedTime);               // プレイヤーへ接近し射程で停止
	void UpdatePerception(float elapsedTime);           // 毎フレーム1回、知覚を評価
	bool IsPlayerVisible() const { return perceptVisible; }
	bool IsSearching() const;                           // 見失い後の探索中か
	void SearchUpdate(float elapsedTime);               // 最後に見た位置へ→見回す
	bool CanAttack();                     // 射程内＆クールダウン明けなら true（判定用）
	void BeginAttack();                   // 攻撃モーション開始
	bool AttackUpdate(float elapsedTime); // 攻撃中の更新。1回分終わっ
private:
	enum class State { Idle, Walk };

	void PlayState(State s, bool force);
	void BuildBehaviorTree();
	void FaceTowards(float elapsedTime, float dirX, float dirZ);
	static float RandomRange(float mn, float mx);
	void Die();
	bool RayHitSelf(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDist, float& outT);
	void Register();
	void Unregister();
	static float QuatToYaw(const DirectX::XMFLOAT4& q);
	void DealDamageToPlayer(); // 命中時にプレイヤーへダメージ

	// ---- 参照 ----
	std::weak_ptr<Actor> playerRef;
	ModelComponent* modelComp = nullptr;
	AnimatorComponent* animator = nullptr;

	// ---- アニメ index ----
	int idleIndex = -1;
	int walkIndex = -1;
	int fallIndex = -1;
	int attackIndex = -1;

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

	// ---- Behavior tree / patrol ----
	BehaviorTree tree;
	DirectX::XMFLOAT3 homePos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 target = { 0.0f, 0.0f, 0.0f };
	float wanderRadius = 8.0f;  // 徘徊する範囲
	float arriveRadius = 0.5f;  // 到着とみなす距離
	float idleMin = 1.0f;       // 待機時間の最小
	float idleMax = 3.0f;       // 待機時間の最大

	// ---- Perception (Step 3) ----
	CollisionMesh stageCollision;                         // ステージの当たり（視線判定に使用）
	float fovDegrees = 90.0f;                             // 視野角（度）
	float eyeHeight = 1.5f;                               // 目の高さ
	DirectX::XMFLOAT3 lastKnownPos = { 0.0f, 0.0f, 0.0f }; // 最後に見た位置（Step4で使用）

	// ---- debug (perception) ----
	float dbgDist = 0.0f;
	bool  dbgInRange = false;
	bool  dbgInFov = false;
	bool  dbgLosClear = false;
	bool  dbgCanSee = false;
	bool  perceptVisible = false;   // 今フレーム、実際に見えているか
	float searchTimer = 0.0f;       // 探索の残り時間
	float searchDuration = 5.0f;    // 見失ってから探索する秒数
	float searchTurnSpeed = 1.5f;   // 見回すときの回転速度[rad/s]

	// ---- Attack (Step 5) ----
	float attackRange = 2.5f;      // この距離以内で攻撃
	float attackInterval = 1.5f;   // 攻撃の間隔（クールダウン）
	float attackDuration = 0.8f;   // 攻撃モーションの長さ（クリップ長に合わせる）
	float attackTimer = 0.0f;      // クールダウン残り
	float attackAnimTimer = 0.0f;  // 現在の攻撃モーション残り
	bool  attacking = false;
	float attackDamage = 10.0f;  // 1発のダメージ量
	bool  damageDealt = false;   // この攻撃で命中処理済みか
};