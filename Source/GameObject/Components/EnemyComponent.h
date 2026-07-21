#pragma once

#include <memory>
#include <DirectXMath.h>
#include "GameObject/Component.h"
#include "AI/BehaviorTree.h"
#include "Physics/CollisionMesh.h"
#include <vector>
#include <string>

class ModelComponent;
class AnimatorComponent;
class NavGrid;

// Enemy AI: patrol -> perceive (sight/LOS) -> chase -> attack -> search.
// Decision-making is driven by a BehaviorTree; this component is the "body":
// movement, animation, hit sphere for the player's gun, and HP/death.
// The nav mesh is owned by the stage (SceneGame) and shared via SetNavGrid().
class EnemyComponent : public Component
{
public:
	explicit EnemyComponent(std::shared_ptr<Actor> player);
	~EnemyComponent() override;

	const char* GetName() const override { return "Enemy"; }

	void Start() override;
	void Update(float elapsedTime) override;
	void OnGUI() override;

	// Damage taken from the player's gun.
	void ApplyDamage(float dmg);

	// Ray vs all enemies (sphere). Returns the nearest hit enemy, or nullptr.
	static EnemyComponent* Raycast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDist, float& outDist);

	void SaveSettings();
	void LoadSettings();

	// ---- Shared resources injected by the scene ----
	void SetStageCollision(const CollisionMesh& mesh);
	void SetNavGrid(NavGrid* grid) { navGrid = grid; }

	// ---- AI action API (behavior-tree leaves call these) ----
	void  SetWanderTarget();                 // pick a random patrol target around home
	bool  MoveToTarget(float elapsedTime);   // move toward target; true when arrived
	void  PlayWalk();
	void  PlayIdle();
	float NextIdleDuration();                // random idle wait time

	void  UpdatePerception(float elapsedTime); // evaluate sight once per frame (cached)
	bool  IsPlayerVisible() const { return perceptVisible; }
	bool  IsSearching() const;                 // lost sight but still within search window
	void  ChaseUpdate(float elapsedTime);      // approach the player, stop at stop distance
	void  SearchUpdate(float elapsedTime);     // go to last known position, then look around

	bool  CanAttack();                         // in range and off cooldown
	void  BeginAttack();                       // start the attack motion
	bool  AttackUpdate(float elapsedTime);     // advance attack; true when the motion finished

private:
	enum class State { Idle, Walk };

	void  PlayState(State s, bool force);
	void  BuildBehaviorTree();
	void  FaceTowards(float elapsedTime, float dirX, float dirZ);
	bool  CanSeePlayer();                       // range + FOV + line-of-sight (updates debug fields)
	void  DealDamageToPlayer();
	void  Die();
	bool  RayHitSelf(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& dir, float maxDist, float& outT);
	void  Register();
	void  Unregister();
	static float RandomRange(float mn, float mx);
	static float QuatToYaw(const DirectX::XMFLOAT4& q);
	bool MoveTowards(const DirectX::XMFLOAT3& worldTarget, float elapsedTime, float stopDist);
	DirectX::XMFLOAT3 CollideAndSlide(const DirectX::XMFLOAT3& pos, float moveX, float moveZ);

	// ---- Model switching (inspector) ----
	void  ResolveAnimationIndices();          // (re)read clip indices (IDLE/RUN/DEATH/PUNCH*) from the current model
	void  ChangeModel(const char* filename);  // inspector: swap the model and refresh animation state safely
	void  ScanModelAssets();                  // gather selectable .glb/.gltf under Data/Model for the inspector list

	float MeasureModelHeight() const;         // natural (un-scaled) model height, for auto-fit on swap

	// ---- References ----
	std::weak_ptr<Actor> playerRef;
	ModelComponent* modelComp = nullptr;
	AnimatorComponent* animator = nullptr;

	// ---- Model switching (inspector) ----
	std::vector<std::string> modelAssetPaths;   // cached candidate models (Data/Model/**.glb/.gltf)
	char modelFilter[64] = { 0 };               // inspector search box text

	// ---- Animation indices ----
	int idleIndex = -1;
	int walkIndex = -1;
	int fallIndex = -1;
	int attackIndex = -1;   // currently playing punch (points at L or R)
	int attackIndexL = -1;  // PUNCHLEFT
	int attackIndexR = -1;  // PUNCHRIGHT
	bool nextPunchRight = false; // toggles each attack

	// ---- State ----
	State state = State::Idle;
	float currentYaw = 0.0f;

	bool  dead = false;
	float deathTimer = 0.0f;
	float deathTime = 2.0f;

	// ---- Stats ----
	float hp = 100.0f, maxHp = 100.0f;

	// ---- Hit sphere (player's gun) ----
	float sphereCenterY = 1.0f;
	float sphereRadius = 1.0f;

	// ---- Movement ----
	float detectRange = 20.0f;
	float stopDistance = 2.0f;
	float moveSpeed = 2.5f;
	float turnSpeed = 8.0f;
	float blendTime = 0.2f;

	// ---- Behavior tree / patrol ----
	BehaviorTree tree;
	DirectX::XMFLOAT3 homePos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 target = { 0.0f, 0.0f, 0.0f };
	float wanderRadius = 8.0f;
	float arriveRadius = 0.5f;
	float idleMin = 1.0f;
	float idleMax = 3.0f;

	// ---- Perception ----
	CollisionMesh stageCollision;
	float fovDegrees = 90.0f;
	float eyeHeight = 1.5f;
	DirectX::XMFLOAT3 lastKnownPos = { 0.0f, 0.0f, 0.0f };
	bool  perceptVisible = false;
	float searchTimer = 0.0f;
	float searchDuration = 5.0f;
	float searchTurnSpeed = 1.5f;

	// ---- Attack ----
	float attackRange = 2.5f;
	float attackInterval = 1.5f;   // cooldown between attacks
	float attackHitTiming = 0.4f;  // normalized time in the anim when the hit lands
	float attackTimer = 0.0f;      // cooldown countdown
	float attackDamage = 10.0f;
	bool  attacking = false;
	bool  damageDealt = false;

	// ---- Debug (perception) ----
	float dbgDist = 0.0f;
	bool  dbgInRange = false;
	bool  dbgInFov = false;
	bool  dbgLosClear = false;
	bool  dbgCanSee = false;

	// ---- Navigation (nav mesh is shared; owned by the stage) ----
	NavGrid* navGrid = nullptr;
	float collisionRadius = 0.4f;   // movement collision radius
	float collisionCenterY = 1.0f;  // height of the collision sphere center (waist)
	float stuckTimer = 0.0f;
	float stuckLimit = 0.5f;        // seconds jammed before giving up this tick
	std::vector<DirectX::XMFLOAT3> path;   // current path (world space)
	int   pathIndex = 0;
	float repathTimer = 0.0f;
	float repathInterval = 0.3f;    // path recompute interval
	float waypointRadius = 0.6f;    // distance to count a waypoint as reached
	DirectX::XMFLOAT3 lastPathTarget = { 0.0f, 0.0f, 0.0f };

	// ---- Ground snap (walk on the floor, UE/OW-like) ----
	bool  groundSnap = true;
	float groundProbeUp = 1.5f;
	float groundProbeDown = 4.0f;

	// ---- Obstacle avoidance (whisker probes vs the collision mesh) ----
	bool  avoidEnable = true;
	float avoidProbeDist = 2.0f;
	float avoidAlignWeight = 1.5f;

	// ---- Steering smoothing (reduce choppy turning) ----
	float steerSmooth = 8.0f;
	float moveDirX = 0.0f;
	float moveDirZ = 1.0f;
};