#include <imgui.h>
#include <windows.h>   // OutputDebugStringA
#include <cstdio>      // sprintf_s
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

		ResolveAnimationIndices();
	}

	LoadSettings();
	Register();
	PlayState(State::Idle, true);

	if (owner) homePos = owner->GetPosition();
	BuildBehaviorTree();
	// Nav mesh is built and owned by the stage; it is injected via SetNavGrid().
}

// Read the animation clip indices for the current model. Called on Start and
// again whenever the model is swapped (indices are per-model).
void EnemyComponent::ResolveAnimationIndices()
{
	idleIndex = walkIndex = fallIndex = -1;
	attackIndexL = attackIndexR = attackIndex = -1;

	if (modelComp && modelComp->GetModel())
	{
		std::shared_ptr<Model> model = modelComp->GetModel();
		idleIndex = model->GetAnimationIndex("IDLE");
		walkIndex = model->GetAnimationIndex("RUN");        // no WALK clip -> use RUN
		fallIndex = model->GetAnimationIndex("DEATH");      // -1 if none
		attackIndexL = model->GetAnimationIndex("PUNCHLEFT");
		attackIndexR = model->GetAnimationIndex("PUNCHRIGHT");
		attackIndex = (attackIndexR >= 0) ? attackIndexR : attackIndexL;
	}
}

// Natural (un-scaled) height of the current model, measured from the bind-pose
// node transforms. Used to keep a swapped model at a consistent on-screen size
// no matter how the source .glb was authored.
float EnemyComponent::MeasureModelHeight() const
{
	if (!modelComp) return 0.0f;
	std::shared_ptr<Model> model = modelComp->GetModel();
	if (!model) return 0.0f;

	float minY = FLT_MAX;
	float maxY = -FLT_MAX;
	for (const Model::Mesh& mesh : model->GetMeshes())
	{
		DirectX::XMMATRIX nodeW = DirectX::XMLoadFloat4x4(&mesh.node->worldTransform);
		for (const Model::Vertex& v : mesh.vertices)
		{
			DirectX::XMVECTOR p = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&v.position), nodeW);
			float y = DirectX::XMVectorGetY(p);
			minY = (std::min)(minY, y);
			maxY = (std::max)(maxY, y);
		}
	}
	if (maxY <= minY) return 0.0f;
	return maxY - minY;
}

void EnemyComponent::ChangeModel(const char* filename)
{
	std::shared_ptr<Actor> owner = GetActor();
	if (!modelComp || !owner || filename == nullptr || filename[0] == '\0') return;
	if (modelComp->GetModelPath() == filename) return; // already this model

	// Remember the enemy's current on-screen height. Actor scale is tuned per
	// model (Enemy3 uses 0.01 because it is authored huge), so blindly reusing it
	// on a normally-sized model would shrink it to a nearly invisible speck.
	float worldHeight = MeasureModelHeight() * owner->GetScale().y;

	// 1) swap mesh / skeleton / materials
	modelComp->SetModel(filename);

	// 2) re-anchor to the actor (SetModel leaves the new pose at world origin)
	modelComp->ApplyWorldTransform(owner->GetTransform());

	// 3) per-model clip indices + animator rebind
	ResolveAnimationIndices();
	if (animator) animator->RebindModel();
	attacking = false;
	damageDealt = false;
	PlayState(State::Idle, true);

	// 4) auto-fit: scale the actor so the new model keeps the same visible height
	float newHeight = MeasureModelHeight();
	if (newHeight > 1e-4f)
	{
		float target = (worldHeight > 1e-4f) ? worldHeight : 1.8f; // fallback ~human height
		float s = target / newHeight;
		owner->SetScale({ s, s, s });
	}
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