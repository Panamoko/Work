#include <imgui.h>
#include <fstream>
#include <cmath>
#include <cfloat>
#include <vector>
#include <algorithm>
#include <random>
#include "GameObject/Components/EnemyComponent.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/AnimatorComponent.h"
#include "GameObject/Components/HealthComponent.h"
#include "GameObject/Actor.h"
#include "AI/EnemyBehaviors.h"
#include "Physics/CollisionMesh.h"
#include "AI/NavGrid.h"

// ---- Small helpers ----
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

void EnemyComponent::PlayWalk() { PlayState(State::Walk, false); }
void EnemyComponent::PlayIdle() { PlayState(State::Idle, false); }

float EnemyComponent::NextIdleDuration() { return RandomRange(idleMin, idleMax); }

float EnemyComponent::RandomRange(float mn, float mx)
{
	static std::mt19937 rng{ std::random_device{}() };
	std::uniform_real_distribution<float> dist(mn, mx);
	return dist(rng);
}

float EnemyComponent::QuatToYaw(const DirectX::XMFLOAT4& q)
{
	return atan2f(2.0f * (q.w * q.y + q.x * q.z), 1.0f - 2.0f * (q.y * q.y + q.x * q.x));
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

	if (std::shared_ptr<Actor> owner = GetActor())
	{
		DirectX::XMFLOAT4 rot;
		DirectX::XMStoreFloat4(&rot, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, currentYaw, 0.0f));
		owner->SetRotation(rot);
	}
}

// ---- Behavior tree ----
void EnemyComponent::BuildBehaviorTree()
{
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

// ---- Patrol ----
void EnemyComponent::SetWanderTarget()
{
	for (int i = 0; i < 8; ++i)
	{
		float angle = RandomRange(0.0f, DirectX::XM_2PI);
		float r = RandomRange(0.0f, wanderRadius);
		DirectX::XMFLOAT3 t = { homePos.x + cosf(angle) * r, homePos.y, homePos.z + sinf(angle) * r };
		if (!navGrid || !navGrid->IsBuilt() || navGrid->IsWalkable(t)) { target = t; return; }
	}
	target = homePos; // fallback
}

bool EnemyComponent::MoveToTarget(float elapsedTime)
{
	return MoveTowards(target, elapsedTime, arriveRadius);
}