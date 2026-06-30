#include <imgui.h>
#include <fstream>
#include <cmath>
#include <vector>
#include <algorithm>
#include "GameObject/Components/EnemyComponent.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/AnimatorComponent.h"
#include "GameObject/Actor.h"

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
			walkIndex = modelComp->GetModel()->GetAnimationIndex("WALK");
			fallIndex = modelComp->GetModel()->GetAnimationIndex("DEATH");
		}
	}
	LoadSettings();
	Register();
	PlayState(State::Idle, true);
}

// ---------------------------------------------------------
// 更新
// ---------------------------------------------------------
void EnemyComponent::Update(float elapsedTime)
{
	// 死亡中：一定時間後に退場
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

	std::shared_ptr<Actor> owner = GetActor();
	std::shared_ptr<Actor> player = playerRef.lock();
	if (!owner || !player) return;

	DirectX::XMFLOAT3 myPos = owner->GetPosition();
	DirectX::XMFLOAT3 tgtPos = player->GetPosition();

	float dx = tgtPos.x - myPos.x;
	float dz = tgtPos.z - myPos.z;
	float dist = sqrtf(dx * dx + dz * dz);

	// 向く
	if (dist > 0.001f)
	{
		float targetYaw = atan2f(dx, dz);
		float d = targetYaw - currentYaw;
		while (d > DirectX::XM_PI)  d -= DirectX::XM_2PI;
		while (d < -DirectX::XM_PI) d += DirectX::XM_2PI;
		float t = turnSpeed * elapsedTime;
		if (t > 1.0f) t = 1.0f;
		currentYaw += d * t;

		DirectX::XMFLOAT4 rot;
		DirectX::XMStoreFloat4(&rot, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, currentYaw, 0.0f));
		owner->SetRotation(rot);
	}

	// 追跡 or 停止
	if (dist < detectRange && dist > stopDistance)
	{
		float inv = 1.0f / dist;
		myPos.x += dx * inv * moveSpeed * elapsedTime;
		myPos.z += dz * inv * moveSpeed * elapsedTime;
		owner->SetPosition(myPos);
		PlayState(State::Walk, false);
	}
	else
	{
		PlayState(State::Idle, false);
	}
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
	ImGui::TextDisabled("Hit volume (sphere)");
	ImGui::SliderFloat("Center Y", &sphereCenterY, 0.0f, 3.0f);
	ImGui::SliderFloat("Radius", &sphereRadius, 0.2f, 3.0f);
	if (ImGui::Button("Damage 34")) ApplyDamage(34.0f);

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