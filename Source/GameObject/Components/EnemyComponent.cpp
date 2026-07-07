#include <imgui.h>
#include <fstream>
#include <cmath>
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
#include "Graphics/Renderer/PrimitiveRenderer.h"


// Registry of all live enemies (for the player's gun raycast).
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

void EnemyComponent::Register() { EnemyRegistry().push_back(this); }
void EnemyComponent::Unregister()
{
	auto& v = EnemyRegistry();
	v.erase(std::remove(v.begin(), v.end(), this), v.end());
}

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
			walkIndex = modelComp->GetModel()->GetAnimationIndex("RUN");        // no WALK clip -> use RUN
			fallIndex = modelComp->GetModel()->GetAnimationIndex("DEATH");      // -1 if none (no death anim)
			attackIndexL = modelComp->GetModel()->GetAnimationIndex("PUNCHLEFT");
			attackIndexR = modelComp->GetModel()->GetAnimationIndex("PUNCHRIGHT");
			attackIndex = (attackIndexR >= 0) ? attackIndexR : attackIndexL;   // initial
		}
	}

	LoadSettings();
	Register();
	PlayState(State::Idle, true);

	if (owner) homePos = owner->GetPosition();
	BuildBehaviorTree();
	if (owner) homePos = owner->GetPosition();
	navRefY = homePos.y;   // enemy stands on the ground -> use its feet as reference
	BuildBehaviorTree();
	navGrid.Build(stageCollision, navCellSize, navAgentHeight, navRefY, navFloorTol);
}

void EnemyComponent::Update(float elapsedTime)
{
	if (dead)
	{
		deathTimer -= elapsedTime;
		if (deathTimer <= 0.0f)
		{
			if (std::shared_ptr<Actor> owner = GetActor())
				ActorManager::Instance().Remove(owner);
		}
		return;
	}

	UpdatePerception(elapsedTime); // evaluate sight/cooldowns once
	tree.Update(elapsedTime);      // decide + act (single tick)
}

// ---- Damage / death (enemy's own HP; the player's gun calls ApplyDamage) ----
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
	Unregister();
	if (animator && fallIndex >= 0) animator->Play(fallIndex, 0.1f, false);
	deathTimer = deathTime;
}

// ---- Gun hit test (sphere) ----
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

// ---- Inspector ----
void EnemyComponent::OnGUI()
{
	ImGui::Text("Enemy  (%s)", dead ? "DEAD" : (state == State::Walk ? "WALK" : "IDLE"));
	ImGui::ProgressBar(maxHp > 0.0f ? hp / maxHp : 0.0f, ImVec2(-1, 0));
	ImGui::Text("HP: %.0f / %.0f", hp, maxHp);

	if (ImGui::CollapsingHeader("Movement", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SliderFloat("Detect Range", &detectRange, 1.0f, 50.0f);
		ImGui::SliderFloat("Stop Distance", &stopDistance, 0.5f, 10.0f);
		ImGui::SliderFloat("Move Speed", &moveSpeed, 0.0f, 10.0f);
		ImGui::SliderFloat("Turn Speed", &turnSpeed, 1.0f, 20.0f);
		ImGui::SliderFloat("Blend Time", &blendTime, 0.0f, 0.6f);
	}

	if (ImGui::CollapsingHeader("Patrol"))
	{
		ImGui::SliderFloat("Wander Radius", &wanderRadius, 1.0f, 30.0f);
		ImGui::SliderFloat("Arrive Radius", &arriveRadius, 0.1f, 3.0f);
		ImGui::SliderFloat("Idle Min", &idleMin, 0.0f, 5.0f);
		ImGui::SliderFloat("Idle Max", &idleMax, 0.0f, 8.0f);
	}

	if (ImGui::CollapsingHeader("Perception"))
	{
		ImGui::SliderFloat("FOV (deg)", &fovDegrees, 20.0f, 180.0f);
		ImGui::SliderFloat("Eye Height", &eyeHeight, 0.0f, 3.0f);
		ImGui::Text("Dist to player : %.1f  (range %.0f)", dbgDist, detectRange);
		ImGui::TextColored(dbgInRange ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.4f, 0, 1), "In range  : %s", dbgInRange ? "YES" : "no");
		ImGui::TextColored(dbgInFov ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.4f, 0, 1), "In FOV    : %s", dbgInFov ? "YES" : "no");
		ImGui::TextColored(dbgLosClear ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.4f, 0, 1), "LOS clear : %s", dbgLosClear ? "YES" : "no");
		ImGui::TextColored(dbgCanSee ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.4f, 0, 1), "CAN SEE   : %s", dbgCanSee ? "YES" : "no");
		ImGui::Text("Search timer : %.1f", searchTimer);
	}

	if (ImGui::CollapsingHeader("Navigation"))
	{
		ImGui::Text("Grid built : %s", navGrid.IsBuilt() ? "YES" : "no");
		ImGui::Text("Path nodes : %d", (int)path.size());
		ImGui::SliderFloat("Repath Interval", &repathInterval, 0.1f, 1.0f);

		ImGui::Separator();
		ImGui::SliderFloat("Nav Cell Size", &navCellSize, 0.3f, 2.0f);
		if (ImGui::Button("Rebuild Nav Grid"))
			navGrid.Build(stageCollision, navCellSize, navAgentHeight, navRefY, navFloorTol); // その場で格子を作り直す

		ImGui::SliderFloat("Collision Radius", &collisionRadius, 0.1f, 1.0f);
		ImGui::SliderFloat("Collision Center Y", &collisionCenterY, 0.2f, 2.0f);
		ImGui::SliderFloat("Waypoint Radius", &waypointRadius, 0.15f, 1.0f);
		ImGui::SliderFloat("Waypoint Radius", &waypointRadius, 0.15f, 1.0f);
		ImGui::Checkbox("Ground Snap", &groundSnap);
		ImGui::SliderFloat("Ground Probe Up", &groundProbeUp, 0.2f, 3.0f);
		ImGui::SliderFloat("Ground Probe Down", &groundProbeDown, 0.5f, 8.0f);
		ImGui::SliderFloat("Ground Probe Down", &groundProbeDown, 0.5f, 8.0f);
		ImGui::Checkbox("Avoid Obstacles", &avoidEnable);
		ImGui::SliderFloat("Avoid Probe Dist", &avoidProbeDist, 0.5f, 5.0f);
		ImGui::SliderFloat("Avoid Align Weight", &avoidAlignWeight, 0.0f, 4.0f);
	}

	if (ImGui::CollapsingHeader("Attack"))
	{
		ImGui::SliderFloat("Attack Range", &attackRange, 0.5f, 6.0f);
		ImGui::SliderFloat("Attack Interval", &attackInterval, 0.2f, 5.0f);
		ImGui::SliderFloat("Hit Timing", &attackHitTiming, 0.0f, 1.0f);
		ImGui::SliderFloat("Attack Damage", &attackDamage, 0.0f, 50.0f);
	}

	if (ImGui::CollapsingHeader("Hit volume (sphere)"))
	{
		ImGui::SliderFloat("Center Y", &sphereCenterY, 0.0f, 3.0f);
		ImGui::SliderFloat("Radius", &sphereRadius, 0.2f, 3.0f);
		if (ImGui::Button("Damage 34")) ApplyDamage(34.0f);
	}

	if (ImGui::CollapsingHeader("NavMesh Debug"))
	{
		ImGui::Checkbox("Show NavMesh", &showNav);
		ImGui::SliderFloat("Debug Radius", &navDebugRadius, 3.0f, 60.0f);
		ImGui::Text("Active Cell Size: %.2f  (cells: %d)", navGrid.GetCellSize(), navGrid.CellCount());
		ImGui::Separator();
		ImGui::SliderFloat("Nav Cell Size", &navCellSize, 0.3f, 4.0f);
		ImGui::SliderFloat("Agent Height", &navAgentHeight, 0.3f, 3.0f);
		ImGui::SliderFloat("Ground Ref Y", &navRefY, -20.0f, 20.0f);
		ImGui::SliderFloat("Floor Tolerance", &navFloorTol, 0.05f, 2.0f);
		if (ImGui::Button("Rebuild NavMesh"))
			navGrid.Build(stageCollision, navCellSize, navAgentHeight, navRefY, navFloorTol);
	}

	ImGui::Separator();
	if (ImGui::Button("Save Transform")) SaveSettings();
	ImGui::SameLine();
	if (ImGui::Button("Load Transform")) LoadSettings();
}

// ---- Save / load transform ----
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
	// Root(Priority)
	//  |- Combat (0, visible)   - Priority
	//  |    |- Attack (0, in range & off cooldown)
	//  |    \- Chase  (1)       - approach
	//  |- Search (1, searching) - go to last known pos, look around
	//  \- Patrol (2, looping)   - wander -> idle
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
		if (!navGrid.IsBuilt() || navGrid.IsWalkable(t)) { target = t; return; }
	}
	target = homePos; // fallback
}

bool EnemyComponent::MoveToTarget(float elapsedTime)
{
	return MoveTowards(target, elapsedTime, arriveRadius);
}

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
		// 射程に到達：プレイヤーを向いて待機
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

	// 到着：その場で見回す
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

	// Progress through the actual attack animation (0..1).
	float t = (animator && attackIndex >= 0) ? animator->GetNormalizedTime() : 1.0f;

	// Land the hit once, at the configured moment of the swing.
	if (!damageDealt && t >= attackHitTiming)
	{
		DealDamageToPlayer();
		damageDealt = true;
	}

	// Finish when the animation has played out.
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

// Move toward worldTarget along the nav path. Returns true when within
// 'stopDist' of the final target. Falls back to a straight line if no path.
bool EnemyComponent::MoveTowards(const DirectX::XMFLOAT3& worldTarget, float elapsedTime, float stopDist)
{
	std::shared_ptr<Actor> owner = GetActor();
	if (!owner) return true;

	DirectX::XMFLOAT3 pos = owner->GetPosition();

	// Reached the final target?
	float fdx = worldTarget.x - pos.x;
	float fdz = worldTarget.z - pos.z;
	if (fdx * fdx + fdz * fdz <= stopDist * stopDist)
	{
		path.clear();
		stuckTimer = 0.0f;
		return true;
	}

	// Recompute the A* path on a timer, or when the target moved a lot.
	float tdx = worldTarget.x - lastPathTarget.x;
	float tdz = worldTarget.z - lastPathTarget.z;
	bool targetMoved = (tdx * tdx + tdz * tdz) > (2.0f * 2.0f);
	repathTimer -= elapsedTime;
	if (repathTimer <= 0.0f || path.empty() || targetMoved)
	{
		repathTimer = repathInterval;
		lastPathTarget = worldTarget;
		pathIndex = 0;
		if (!navGrid.IsBuilt() || !navGrid.FindPath(pos, worldTarget, path))
			path.clear();
	}

	// Pick the current steering target (next waypoint, or straight at the goal).
	DirectX::XMFLOAT3 wp = worldTarget;
	if (!path.empty())
	{
		while (pathIndex < (int)path.size())
		{
			float wx = path[pathIndex].x - pos.x;
			float wz = path[pathIndex].z - pos.z;
			if (wx * wx + wz * wz <= waypointRadius * waypointRadius) { ++pathIndex; continue; }
			break;
		}
		wp = (pathIndex < (int)path.size()) ? path[pathIndex] : worldTarget;
	}

	float mdx = wp.x - pos.x;
	float mdz = wp.z - pos.z;
	float dist = sqrtf(mdx * mdx + mdz * mdz);
	if (dist <= 1e-4f) return false;

	// Desired heading (unit).
	float dirX = mdx / dist;
	float dirZ = mdz / dist;

	// --- Obstacle avoidance: probe several headings and pick the best open one ---
	if (avoidEnable && !stageCollision.triangles.empty())
	{
		DirectX::XMFLOAT3 eye = { pos.x, pos.y + collisionCenterY, pos.z };

		auto clearDistance = [&](float ux, float uz) -> float
			{
				DirectX::XMFLOAT3 d = { ux, 0.0f, uz };
				float hd = 0.0f; DirectX::XMFLOAT3 hp, hn;
				if (stageCollision.RayCast(eye, d, avoidProbeDist, hd, hp, hn))
					return hd;
				return avoidProbeDist;
			};

		float straightClear = clearDistance(dirX, dirZ);
		if (straightClear < avoidProbeDist - 0.05f)
		{
			const int HALF = 4; // 2*HALF+1 samples over -90..+90 deg
			const float spread = DirectX::XM_PIDIV2;
			float bestScore = -FLT_MAX, bestX = dirX, bestZ = dirZ;
			for (int k = -HALF; k <= HALF; ++k)
			{
				float ang = (spread / HALF) * (float)k;
				float ca = cosf(ang), sa = sinf(ang);
				float rx = dirX * ca - dirZ * sa;
				float rz = dirX * sa + dirZ * ca;

				float clear = clearDistance(rx, rz);
				float align = rx * dirX + rz * dirZ;
				float score = clear + align * avoidAlignWeight;
				if (score > bestScore)
				{
					bestScore = score;
					bestX = rx; bestZ = rz;
				}
			}
			dirX = bestX; dirZ = bestZ;
		}
	}

	// --- Smooth the heading so turning is not choppy ---
	{
		float t = steerSmooth * elapsedTime;
		if (t > 1.0f) t = 1.0f;
		moveDirX += (dirX - moveDirX) * t;
		moveDirZ += (dirZ - moveDirZ) * t;
		float ml = sqrtf(moveDirX * moveDirX + moveDirZ * moveDirZ);
		if (ml > 1e-4f) { moveDirX /= ml; moveDirZ /= ml; }
	}

	FaceTowards(elapsedTime, moveDirX, moveDirZ);

	float stepX = moveDirX * moveSpeed * elapsedTime;
	float stepZ = moveDirZ * moveSpeed * elapsedTime;

	DirectX::XMFLOAT3 newPos = CollideAndSlide(pos, stepX, stepZ);

	// Ground snap: drop a ray straight down and sit on the floor.
	if (groundSnap && !stageCollision.triangles.empty())
	{
		DirectX::XMFLOAT3 rayOrigin = { newPos.x, newPos.y + groundProbeUp, newPos.z };
		DirectX::XMFLOAT3 rayDir = { 0.0f, -1.0f, 0.0f };
		float gd = 0.0f; DirectX::XMFLOAT3 gp, gn;
		if (stageCollision.RayCast(rayOrigin, rayDir, groundProbeUp + groundProbeDown, gd, gp, gn) && gn.y > 0.4f)
			newPos.y = gp.y;
	}

	// Stuck detection.
	float movedSq = (newPos.x - pos.x) * (newPos.x - pos.x) + (newPos.z - pos.z) * (newPos.z - pos.z);
	float stepSq = stepX * stepX + stepZ * stepZ;
	if (stepSq > 1e-8f && movedSq < stepSq * 0.09f) stuckTimer += elapsedTime;
	else stuckTimer = 0.0f;

	owner->SetPosition(newPos);

	if (stuckTimer > stuckLimit)
	{
		stuckTimer = 0.0f;
		path.clear();
		repathTimer = 0.0f;
	}
	return false;
}

// Move on the XZ plane while sliding along walls (sphere sweep vs the stage).
DirectX::XMFLOAT3 EnemyComponent::CollideAndSlide(const DirectX::XMFLOAT3& pos, float moveX, float moveZ)
{
	DirectX::XMFLOAT3 p = pos;
	if (stageCollision.triangles.empty())
	{
		p.x += moveX; p.z += moveZ;
		return p;
	}

	// Pass 0: move up to the first wall. Pass 1: slide along it.
	for (int pass = 0; pass < 2; ++pass)
	{
		float len = sqrtf(moveX * moveX + moveZ * moveZ);
		if (len < 1e-5f) break;

		DirectX::XMFLOAT3 center = { p.x, p.y + collisionCenterY, p.z };
		DirectX::XMFLOAT3 dir = { moveX / len, 0.0f, moveZ / len };
		float dist = len + collisionRadius;
		DirectX::XMFLOAT3 hitPos, hitNormal;

		if (!stageCollision.SphereCast(center, dir, collisionRadius, dist, hitPos, hitNormal))
		{
			p.x += moveX; p.z += moveZ; // clear: full move
			break;
		}

		// Move only up to just before the wall.
		float allowed = dist - collisionRadius;
		if (allowed < 0.0f) allowed = 0.0f;
		p.x += dir.x * allowed;
		p.z += dir.z * allowed;

		// Project the leftover movement onto the wall (remove the normal part), then retry.
		float leftover = len - allowed;
		float lx = dir.x * leftover;
		float lz = dir.z * leftover;
		float nlen = sqrtf(hitNormal.x * hitNormal.x + hitNormal.z * hitNormal.z);
		if (nlen < 1e-4f) break;
		float nx = hitNormal.x / nlen, nz = hitNormal.z / nlen;
		float dot = lx * nx + lz * nz;
		moveX = lx - dot * nx;
		moveZ = lz - dot * nz;
	}
	return p;
}

void EnemyComponent::DebugDrawNav(PrimitiveRenderer* prim)
{
	if (!showNav || !prim || !navGrid.IsBuilt()) return;

	// Center the overlay on the player so you can inspect wherever you look.
	std::shared_ptr<Actor> player = playerRef.lock();
	std::shared_ptr<Actor> owner = GetActor();
	DirectX::XMFLOAT3 c =
		player ? player->GetPosition() :
		owner ? owner->GetPosition() : DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };

	std::vector<DirectX::XMFLOAT3> walk, block;
	navGrid.CollectDebugCells(c, navDebugRadius, walk, block, 0.06f);

	const DirectX::XMFLOAT4 green = { 0.10f, 0.90f, 0.30f, 0.35f };
	const DirectX::XMFLOAT4 red = { 0.90f, 0.15f, 0.15f, 0.30f };
	for (const DirectX::XMFLOAT3& v : walk)  prim->AddVertex(v, green);
	for (const DirectX::XMFLOAT3& v : block) prim->AddVertex(v, red);
}

