#include <imgui.h>
#include <fstream>
#include <cmath>
#include <vector>
#include <algorithm>
#include "GameObject/Components/EnemyComponent.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/AnimatorComponent.h"
#include "GameObject/Actor.h"
#include <random>                 
#include "AI/EnemyBehaviors.h"    
#include "Physics/CollisionMesh.h"
#include "GameObject/Components/HealthComponent.h"

// 生存中の敵レジストリ（このTU内で共有）
static std::vector<EnemyComponent*>& EnemyRegistry()
{
	static std::vector<EnemyComponent*> reg;
	return reg;
}

EnemyComponent::EnemyComponent(std::shared_ptr<Actor> player)
	: playerRef(player)
{
}

EnemyComponent::~EnemyComponent()
{
	Unregister();
}

void EnemyComponent::Register()
{
	EnemyRegistry().push_back(this);
}

void EnemyComponent::Unregister()
{
	auto& v = EnemyRegistry();
	v.erase(std::remove(v.begin(), v.end(), this), v.end());
}

// ---------------------------------------------------------
// 開始
// ---------------------------------------------------------
void EnemyComponent::Start()
{
	std::shared_ptr<Actor> owner = GetActor();
	if (owner)
	{
		modelComp = owner->GetComponent<ModelComponent>().get();
		animator = owner->GetComponent<AnimatorComponent>().get();
		currentYaw = QuatToYaw(owner->GetRotation());

		if (modelComp && modelComp->GetModel())
		{
			idleIndex = modelComp->GetModel()->GetAnimationIndex("IDLE");
			walkIndex = modelComp->GetModel()->GetAnimationIndex("RUN");        // 移動＝RUN（WALKが無いため）
			fallIndex = modelComp->GetModel()->GetAnimationIndex("DEATH");      // 無ければ -1（死亡アニメ無し）
			attackIndex = modelComp->GetModel()->GetAnimationIndex("PUNCHRIGHT"); // 攻撃＝パンチ
		}
	}
	LoadSettings();
	Register();
	PlayState(State::Idle, true);

	if (owner) homePos = owner->GetPosition(); 
	BuildBehaviorTree();                       
}

// ---------------------------------------------------------
// 更新
// ---------------------------------------------------------
void EnemyComponent::Update(float elapsedTime)
{
	if (dead)
	{
		deathTimer -= elapsedTime;
		if (deathTimer <= 0.0f)
		{
			std::shared_ptr<Actor> owner = GetActor();
			if (owner) ActorManager::Instance().Remove(owner);
		}
		return;
	}

	UpdatePerception(elapsedTime); // 知覚を1回評価（判定はこのキャッシュを見る）
	tree.Update(elapsedTime);

	tree.Update(elapsedTime);
}

// ---------------------------------------------------------
// 被弾・死亡
// ---------------------------------------------------------
void EnemyComponent::ApplyDamage(float dmg)
{
	if (dead) return;
	hp -= dmg;
	if (hp <= 0.0f)
	{
		hp = 0.0f;
		Die();
	}
}

void EnemyComponent::Die()
{
	if (dead) return;
	dead = true;
	Unregister(); // 以後は撃たれない
	if (animator && fallIndex >= 0) animator->Play(fallIndex, 0.1f, false); // 倒れる風
	deathTimer = deathTime;
}

// レイ vs 敵の球（dir は正規化済み前提）
bool EnemyComponent::RayHitSelf(const DirectX::XMFLOAT3& o, const DirectX::XMFLOAT3& d, float maxDist, float& outT)
{
	std::shared_ptr<Actor> owner = GetActor();
	if (!owner) return false;

	DirectX::XMFLOAT3 p = owner->GetPosition();
	float cx = p.x, cy = p.y + sphereCenterY, cz = p.z;

	float Lx = cx - o.x, Ly = cy - o.y, Lz = cz - o.z;
	float tca = Lx * d.x + Ly * d.y + Lz * d.z;
	float d2 = (Lx * Lx + Ly * Ly + Lz * Lz) - tca * tca;
	float r2 = sphereRadius * sphereRadius;
	if (d2 > r2) return false;

	float thc = sqrtf(r2 - d2);
	float t0 = tca - thc;
	float t1 = tca + thc;
	float t = (t0 >= 0.0f) ? t0 : t1;
	if (t < 0.0f || t > maxDist) return false;

	outT = t;
	return true;
}

EnemyComponent* EnemyComponent::Raycast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDist, float& outDist)
{
	float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
	if (len < 1e-6f) return nullptr;
	DirectX::XMFLOAT3 d = { dir.x / len, dir.y / len, dir.z / len };

	EnemyComponent* best = nullptr;
	float bestT = maxDist;
	for (EnemyComponent* e : EnemyRegistry())
	{
		if (e->dead) continue;
		float t = 0.0f;
		if (e->RayHitSelf(origin, d, maxDist, t) && t < bestT)
		{
			bestT = t;
			best = e;
		}
	}
	outDist = bestT;
	return best;
}

// ---------------------------------------------------------
// インスペクタ
// ---------------------------------------------------------
void EnemyComponent::OnGUI()
{
	ImGui::Text("Enemy  (%s)", dead ? "DEAD" : (state == State::Walk ? "WALK" : "IDLE"));
	ImGui::ProgressBar(maxHp > 0.0f ? hp / maxHp : 0.0f, ImVec2(-1, 0));
	ImGui::Text("HP: %.0f / %.0f", hp, maxHp);

	ImGui::Separator();
	ImGui::SliderFloat("Detect Range", &detectRange, 1.0f, 50.0f);
	ImGui::SliderFloat("Stop Distance", &stopDistance, 0.5f, 10.0f);
	ImGui::SliderFloat("Move Speed", &moveSpeed, 0.0f, 10.0f);
	ImGui::SliderFloat("Turn Speed", &turnSpeed, 1.0f, 20.0f);
	ImGui::SliderFloat("Blend Time", &blendTime, 0.0f, 0.6f);
	ImGui::Separator();
	ImGui::TextDisabled("Patrol (wander)");
	ImGui::SliderFloat("Wander Radius", &wanderRadius, 1.0f, 30.0f);
	ImGui::SliderFloat("Arrive Radius", &arriveRadius, 0.1f, 3.0f);
	ImGui::SliderFloat("Idle Min", &idleMin, 0.0f, 5.0f);
	ImGui::SliderFloat("Idle Max", &idleMax, 0.0f, 8.0f);

	ImGui::Separator();
	ImGui::TextDisabled("Perception (sight)");
	ImGui::SliderFloat("FOV (deg)", &fovDegrees, 20.0f, 180.0f);
	ImGui::SliderFloat("Eye Height", &eyeHeight, 0.0f, 3.0f);

	ImGui::SliderFloat("Eye Height", &eyeHeight, 0.0f, 3.0f);

	ImGui::Separator();
	ImGui::TextDisabled("Perception debug");
	ImGui::Text("Dist to player : %.1f  (range %.0f)", dbgDist, detectRange);
	ImGui::TextColored(dbgInRange ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.4f, 0, 1), "In range  : %s", dbgInRange ? "YES" : "no");
	ImGui::TextColored(dbgInFov ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.4f, 0, 1), "In FOV    : %s", dbgInFov ? "YES" : "no");
	ImGui::TextColored(dbgLosClear ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.4f, 0, 1), "LOS clear : %s", dbgLosClear ? "YES" : "no");
	ImGui::TextColored(dbgCanSee ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.4f, 0, 1), "CAN SEE   : %s", dbgCanSee ? "YES" : "no");
	ImGui::Text("Search timer : %.1f", searchTimer); // ← 追加
	ImGui::Separator();
	ImGui::TextDisabled("Attack");
	ImGui::SliderFloat("Attack Range", &attackRange, 0.5f, 6.0f);
	ImGui::SliderFloat("Attack Interval", &attackInterval, 0.2f, 5.0f);
	ImGui::SliderFloat("Attack Duration", &attackDuration, 0.1f, 3.0f);
	ImGui::Separator();
	ImGui::TextDisabled("Hit volume (sphere)");
	ImGui::SliderFloat("Center Y", &sphereCenterY, 0.0f, 3.0f);
	ImGui::SliderFloat("Radius", &sphereRadius, 0.2f, 3.0f);
	if (ImGui::Button("Damage 34")) ApplyDamage(34.0f);
	ImGui::SliderFloat("Attack Duration", &attackDuration, 0.1f, 3.0f);
	ImGui::SliderFloat("Attack Damage", &attackDamage, 0.0f, 50.0f); // ← 追加
	ImGui::Separator();
	if (ImGui::Button("Save Transform")) SaveSettings();
	ImGui::SameLine();
	if (ImGui::Button("Load Transform")) LoadSettings();
}

// ---------------------------------------------------------
// 保存／読込
// ---------------------------------------------------------
void EnemyComponent::SaveSettings()
{
	std::shared_ptr<Actor> owner = GetActor();
	if (!owner) return;
	std::ofstream ofs("EnemySettings.txt");
	if (!ofs) return;

	const DirectX::XMFLOAT3& p = owner->GetPosition();
	const DirectX::XMFLOAT4& r = owner->GetRotation();
	const DirectX::XMFLOAT3& s = owner->GetScale();
	ofs << p.x << " " << p.y << " " << p.z << "\n";
	ofs << r.x << " " << r.y << " " << r.z << " " << r.w << "\n";
	ofs << s.x << " " << s.y << " " << s.z << "\n";
}

void EnemyComponent::LoadSettings()
{
	std::shared_ptr<Actor> owner = GetActor();
	if (!owner) return;
	std::ifstream ifs("EnemySettings.txt");
	if (!ifs) return;

	DirectX::XMFLOAT3 p, s;
	DirectX::XMFLOAT4 r;
	ifs >> p.x >> p.y >> p.z;
	ifs >> r.x >> r.y >> r.z >> r.w;
	ifs >> s.x >> s.y >> s.z;
	owner->SetPosition(p);
	owner->SetRotation(r);
	owner->SetScale(s);
	currentYaw = QuatToYaw(r);
}

// ---------------------------------------------------------
// 内部ヘルパー
// ---------------------------------------------------------
void EnemyComponent::PlayState(State s, bool force)
{
	if (!force && s == state) return;
	state = s;
	if (animator)
	{
		int idx = (s == State::Walk) ? walkIndex : idleIndex;
		if (idx >= 0) animator->Play(idx, blendTime, true);
	}
}

float EnemyComponent::QuatToYaw(const DirectX::XMFLOAT4& q)
{
	return atan2f(2.0f * (q.w * q.y + q.x * q.z), 1.0f - 2.0f * (q.y * q.y + q.x * q.x));
}

// ---------------------------------------------------------
// AI action API / behavior tree
// ---------------------------------------------------------
void EnemyComponent::BuildBehaviorTree()
{
	// Root(Priority)
	//  ├ Combat (0, 判定:見えている) ─ Priority
	//  │   ├ Attack (0, 判定:射程内＆CD明け) : 攻撃
	//  │   └ Chase  (1)                      : 接近
	//  ├ Search (1, 判定:探索中)             : 最後に見た位置へ→見回す
	//  └ Patrol (2, SequentialLooping)       : Wander → Idle
	tree.AddNode("", "Root", 0, SelectRule::Priority, nullptr, nullptr);

	tree.AddNode("Root", "Combat", 0, SelectRule::Priority,
		std::make_unique<SightJudgment>(this), nullptr);
	tree.AddNode("Combat", "Attack", 0, SelectRule::None,
		std::make_unique<AttackJudgment>(this), std::make_unique<AttackAction>(this));
	tree.AddNode("Combat", "Chase", 1, SelectRule::None,
		nullptr, std::make_unique<ChaseAction>(this));

	tree.AddNode("Root", "Search", 1, SelectRule::None,
		std::make_unique<SearchJudgment>(this), std::make_unique<SearchAction>(this));

	tree.AddNode("Root", "Patrol", 2, SelectRule::SequentialLooping, nullptr, nullptr);
	tree.AddNode("Patrol", "Wander", 0, SelectRule::None, nullptr, std::make_unique<WanderAction>(this));
	tree.AddNode("Patrol", "Idle", 1, SelectRule::None, nullptr, std::make_unique<IdleAction>(this));
}

void EnemyComponent::SetWanderTarget()
{
	float angle = RandomRange(0.0f, DirectX::XM_2PI);
	float r = RandomRange(0.0f, wanderRadius);
	target.x = homePos.x + cosf(angle) * r;
	target.y = homePos.y;
	target.z = homePos.z + sinf(angle) * r;
}

bool EnemyComponent::MoveToTarget(float elapsedTime)
{
	std::shared_ptr<Actor> owner = GetActor();
	if (!owner) return true;

	DirectX::XMFLOAT3 pos = owner->GetPosition();
	float dx = target.x - pos.x;
	float dz = target.z - pos.z;
	float distSq = dx * dx + dz * dz;
	if (distSq <= arriveRadius * arriveRadius) return true; // 到着

	float dist = sqrtf(distSq);
	FaceTowards(elapsedTime, dx, dz);

	float inv = 1.0f / dist;
	pos.x += dx * inv * moveSpeed * elapsedTime;
	pos.z += dz * inv * moveSpeed * elapsedTime;
	owner->SetPosition(pos);
	return false;
}

void EnemyComponent::FaceTowards(float elapsedTime, float dirX, float dirZ)
{
	if (dirX * dirX + dirZ * dirZ < 1e-6f) return;

	float targetYaw = atan2f(dirX, dirZ);
	float d = targetYaw - currentYaw;
	while (d > DirectX::XM_PI)  d -= DirectX::XM_2PI;
	while (d < -DirectX::XM_PI) d += DirectX::XM_2PI;

	float t = turnSpeed * elapsedTime;
	if (t > 1.0f) t = 1.0f;
	currentYaw += d * t;

	std::shared_ptr<Actor> owner = GetActor();
	if (owner)
	{
		DirectX::XMFLOAT4 rot;
		DirectX::XMStoreFloat4(&rot, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, currentYaw, 0.0f));
		owner->SetRotation(rot);
	}
}

void EnemyComponent::PlayWalk() { PlayState(State::Walk, false); }
void EnemyComponent::PlayIdle() { PlayState(State::Idle, false); }

float EnemyComponent::NextIdleDuration() { return RandomRange(idleMin, idleMax); }

float EnemyComponent::RandomRange(float mn, float mx)
{
	static std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> dist(mn, mx);
	return dist(rng);
}

// ---------------------------------------------------------
// Perception / chase (Step 3)
// ---------------------------------------------------------
void EnemyComponent::SetStageCollision(const CollisionMesh& mesh)
{
	stageCollision = mesh; // 視線判定用にコピーして保持
}

bool EnemyComponent::CanSeePlayer()
{
	dbgInRange = dbgInFov = dbgLosClear = dbgCanSee = false;

	std::shared_ptr<Actor> owner = GetActor();
	std::shared_ptr<Actor> player = playerRef.lock();
	if (!owner || !player) return false;

	DirectX::XMFLOAT3 myPos = owner->GetPosition();
	DirectX::XMFLOAT3 plPos = player->GetPosition();
	DirectX::XMFLOAT3 eye = { myPos.x, myPos.y + eyeHeight, myPos.z };
	DirectX::XMFLOAT3 tgt = { plPos.x, plPos.y + eyeHeight, plPos.z };

	float dx = tgt.x - eye.x;
	float dy = tgt.y - eye.y;
	float dz = tgt.z - eye.z;
	float dist = sqrtf(dx * dx + dy * dy + dz * dz);
	dbgDist = dist;

	// ① 距離
	if (dist > detectRange || dist < 1e-4f) return false;
	dbgInRange = true;

	// ② 視野角（水平）
	float horiz = sqrtf(dx * dx + dz * dz);
	if (horiz > 1e-4f)
	{
		float toX = dx / horiz;
		float toZ = dz / horiz;
		float fwdX = sinf(currentYaw);
		float fwdZ = cosf(currentYaw);
		float cosAngle = fwdX * toX + fwdZ * toZ;
		float cosHalfFov = cosf(DirectX::XMConvertToRadians(fovDegrees) * 0.5f);
		if (cosAngle < cosHalfFov) return false;
	}
	dbgInFov = true;

	// ③ 視線（壁）
	if (!stageCollision.triangles.empty())
	{
		DirectX::XMFLOAT3 dir = { dx / dist, dy / dist, dz / dist };
		float hitDist = 0.0f;
		DirectX::XMFLOAT3 hitPos, hitNormal;
		if (stageCollision.RayCast(eye, dir, dist, hitDist, hitPos, hitNormal) && hitDist < dist - 0.1f)
			return false; // 壁に遮られている
	}
	dbgLosClear = true;

	lastKnownPos = plPos;
	dbgCanSee = true;
	return true;
}

void EnemyComponent::ChaseUpdate(float elapsedTime)
{
	std::shared_ptr<Actor> owner = GetActor();
	std::shared_ptr<Actor> player = playerRef.lock();
	if (!owner || !player) return;

	DirectX::XMFLOAT3 pos = owner->GetPosition();
	DirectX::XMFLOAT3 plPos = player->GetPosition();
	float dx = plPos.x - pos.x;
	float dz = plPos.z - pos.z;
	float dist = sqrtf(dx * dx + dz * dz);

	FaceTowards(elapsedTime, dx, dz); // 常にプレイヤーを向く

	if (dist > stopDistance)
	{
		float inv = (dist > 1e-4f) ? 1.0f / dist : 0.0f;
		pos.x += dx * inv * moveSpeed * elapsedTime;
		pos.z += dz * inv * moveSpeed * elapsedTime;
		owner->SetPosition(pos);
		PlayWalk();
	}
	else
	{
		PlayIdle(); // 射程に到達（攻撃は Step5）
	}
}

void EnemyComponent::UpdatePerception(float elapsedTime)
{
	perceptVisible = CanSeePlayer();
	if (perceptVisible) searchTimer = searchDuration;
	else if (searchTimer > 0.0f) searchTimer -= elapsedTime;

	if (attackTimer > 0.0f) attackTimer -= elapsedTime; // ← 追加：攻撃クールダウン
}

bool EnemyComponent::IsSearching() const
{
	return !perceptVisible && searchTimer > 0.0f;
}

void EnemyComponent::SearchUpdate(float elapsedTime)
{
	std::shared_ptr<Actor> owner = GetActor();
	if (!owner) return;

	DirectX::XMFLOAT3 pos = owner->GetPosition();
	float dx = lastKnownPos.x - pos.x;
	float dz = lastKnownPos.z - pos.z;
	float dist = sqrtf(dx * dx + dz * dz);

	if (dist > arriveRadius)
	{
		// 最後に見た位置へ向かう
		FaceTowards(elapsedTime, dx, dz);
		float inv = (dist > 1e-4f) ? 1.0f / dist : 0.0f;
		pos.x += dx * inv * moveSpeed * elapsedTime;
		pos.z += dz * inv * moveSpeed * elapsedTime;
		owner->SetPosition(pos);
		PlayWalk();
	}
	else
	{
		// 到着：その場で見回す（ゆっくり回転してプレイヤーを探す）
		currentYaw += searchTurnSpeed * elapsedTime;
		DirectX::XMFLOAT4 rot;
		DirectX::XMStoreFloat4(&rot, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, currentYaw, 0.0f));
		owner->SetRotation(rot);
		PlayIdle();
	}
}

bool EnemyComponent::CanAttack()
{
	if (attackTimer > 0.0f) return false; // クールダウン中

	std::shared_ptr<Actor> owner = GetActor();
	std::shared_ptr<Actor> player = playerRef.lock();
	if (!owner || !player) return false;

	DirectX::XMFLOAT3 a = owner->GetPosition();
	DirectX::XMFLOAT3 b = player->GetPosition();
	float dx = b.x - a.x;
	float dz = b.z - a.z;
	return (dx * dx + dz * dz) <= attackRange * attackRange;
}

void EnemyComponent::BeginAttack()
{
	if (animator && attackIndex >= 0) animator->Play(attackIndex, blendTime, false);
	attacking = true;
	attackAnimTimer = attackDuration;
	damageDealt = false; // ← 追加
}

bool EnemyComponent::AttackUpdate(float elapsedTime)
{
	std::shared_ptr<Actor> owner = GetActor();
	std::shared_ptr<Actor> player = playerRef.lock();
	if (owner && player)
	{
		DirectX::XMFLOAT3 a = owner->GetPosition();
		DirectX::XMFLOAT3 b = player->GetPosition();
		FaceTowards(elapsedTime, b.x - a.x, b.z - a.z); // 攻撃中もプレイヤーを向く
	}

	attackAnimTimer -= elapsedTime;

	// モーション中盤で1回だけ命中判定（この瞬間に射程内なら当たる＝避けられる余地あり）
	if (!damageDealt && attackAnimTimer <= attackDuration * 0.5f)
	{
		DealDamageToPlayer();
		damageDealt = true;
	}

	if (attackAnimTimer <= 0.0f)
	{
		attacking = false;
		attackTimer = attackInterval; // クールダウン開始
		return true;                  // 攻撃1回分が完了
	}
	return false;
}

void EnemyComponent::DealDamageToPlayer()
{
	std::shared_ptr<Actor> owner = GetActor();
	std::shared_ptr<Actor> player = playerRef.lock();
	if (!owner || !player) return;

	// 命中の瞬間も射程内か（離れていれば空振り）
	DirectX::XMFLOAT3 a = owner->GetPosition();
	DirectX::XMFLOAT3 b = player->GetPosition();
	float dx = b.x - a.x;
	float dz = b.z - a.z;
	if (dx * dx + dz * dz > attackRange * attackRange) return;

	if (auto health = player->GetComponent<HealthComponent>())
	{
		health->ApplyDamage(attackDamage);
	}
}