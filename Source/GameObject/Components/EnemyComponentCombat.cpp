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

// ---- Perception ----
void EnemyComponent::SetStageCollision(const CollisionMesh& mesh)
{
	stageCollision = mesh;
}

void EnemyComponent::UpdatePerception(float elapsedTime)
{
	perceptVisible = CanSeePlayer();
	if (perceptVisible) searchTimer = searchDuration;
	else if (searchTimer > 0.0f) searchTimer -= elapsedTime;

	if (attackTimer > 0.0f) attackTimer -= elapsedTime;
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

	float dx = tgt.x - eye.x, dy = tgt.y - eye.y, dz = tgt.z - eye.z;
	float dist = sqrtf(dx * dx + dy * dy + dz * dz);
	dbgDist = dist;

	if (dist > detectRange || dist < 1e-4f) return false;
	dbgInRange = true;

	float horiz = sqrtf(dx * dx + dz * dz);
	if (horiz > 1e-4f)
	{
		float toX = dx / horiz, toZ = dz / horiz;
		float fwdX = sinf(currentYaw), fwdZ = cosf(currentYaw);
		float cosAngle = fwdX * toX + fwdZ * toZ;
		float cosHalfFov = cosf(DirectX::XMConvertToRadians(fovDegrees) * 0.5f);
		if (cosAngle < cosHalfFov) return false;
	}
	dbgInFov = true;

	if (!stageCollision.triangles.empty())
	{
		DirectX::XMFLOAT3 dir = { dx / dist, dy / dist, dz / dist };
		float hitDist = 0.0f;
		DirectX::XMFLOAT3 hitPos, hitNormal;
		if (stageCollision.RayCast(eye, dir, dist, hitDist, hitPos, hitNormal) && hitDist < dist - 0.1f)
			return false; // blocked by a wall
	}
	dbgLosClear = true;

	lastKnownPos = plPos;
	dbgCanSee = true;
	return true;
}

bool EnemyComponent::IsSearching() const
{
	return !perceptVisible && searchTimer > 0.0f;
}

// ---- Chase / search ----
void EnemyComponent::ChaseUpdate(float elapsedTime)
{
	std::shared_ptr<Actor> owner = GetActor();
	std::shared_ptr<Actor> player = playerRef.lock();
	if (!owner || !player) return;

	DirectX::XMFLOAT3 plPos = player->GetPosition();
	if (MoveTowards(plPos, elapsedTime, stopDistance))
	{
		DirectX::XMFLOAT3 pos = owner->GetPosition();
		FaceTowards(elapsedTime, plPos.x - pos.x, plPos.z - pos.z);
		PlayIdle();
	}
	else
	{
		PlayWalk();
	}
}

void EnemyComponent::SearchUpdate(float elapsedTime)
{
	std::shared_ptr<Actor> owner = GetActor();
	if (!owner) return;

	if (!MoveTowards(lastKnownPos, elapsedTime, arriveRadius))
	{
		PlayWalk();
		return;
	}

	currentYaw += searchTurnSpeed * elapsedTime;
	DirectX::XMFLOAT4 rot;
	DirectX::XMStoreFloat4(&rot, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, currentYaw, 0.0f));
	owner->SetRotation(rot);
	PlayIdle();
}

// ---- Attack (synced to the attack animation) ----
bool EnemyComponent::CanAttack()
{
	if (attackTimer > 0.0f) return false;

	std::shared_ptr<Actor> owner = GetActor();
	std::shared_ptr<Actor> player = playerRef.lock();
	if (!owner || !player) return false;

	DirectX::XMFLOAT3 a = owner->GetPosition();
	DirectX::XMFLOAT3 b = player->GetPosition();
	float dx = b.x - a.x, dz = b.z - a.z;
	return (dx * dx + dz * dz) <= attackRange * attackRange;
}

void EnemyComponent::BeginAttack()
{
	// alternate PUNCHLEFT / PUNCHRIGHT each swing
	nextPunchRight = !nextPunchRight;
	attackIndex = nextPunchRight ? attackIndexR : attackIndexL;
	if (attackIndex < 0) attackIndex = (attackIndexR >= 0 ? attackIndexR : attackIndexL);

	if (animator && attackIndex >= 0) animator->Play(attackIndex, blendTime, false);
	attacking = true;
	damageDealt = false;
}

bool EnemyComponent::AttackUpdate(float elapsedTime)
{
	std::shared_ptr<Actor> owner = GetActor();
	std::shared_ptr<Actor> player = playerRef.lock();
	if (owner && player)
	{
		DirectX::XMFLOAT3 a = owner->GetPosition();
		DirectX::XMFLOAT3 b = player->GetPosition();
		FaceTowards(elapsedTime, b.x - a.x, b.z - a.z);
	}

	float t = (animator && attackIndex >= 0) ? animator->GetNormalizedTime() : 1.0f;

	if (!damageDealt && t >= attackHitTiming)
	{
		DealDamageToPlayer();
		damageDealt = true;
	}

	bool finished = (animator && attackIndex >= 0) ? animator->IsAnimationFinished() : true;
	if (finished)
	{
		attacking = false;
		attackTimer = attackInterval; // start cooldown
		return true;
	}
	return false;
}

void EnemyComponent::DealDamageToPlayer()
{
	std::shared_ptr<Actor> owner = GetActor();
	std::shared_ptr<Actor> player = playerRef.lock();
	if (!owner || !player) return;

	DirectX::XMFLOAT3 a = owner->GetPosition();
	DirectX::XMFLOAT3 b = player->GetPosition();
	float dx = b.x - a.x, dz = b.z - a.z;
	if (dx * dx + dz * dz > attackRange * attackRange) return; // player dodged out of range

	if (auto health = player->GetComponent<HealthComponent>())
		health->ApplyDamage(attackDamage);
}