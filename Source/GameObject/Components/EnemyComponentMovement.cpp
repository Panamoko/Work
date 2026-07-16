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

// Move toward worldTarget along the shared nav path. Returns true when within
// 'stopDist' of the final target. Falls back to a straight line if no path.
bool EnemyComponent::MoveTowards(const DirectX::XMFLOAT3& worldTarget, float elapsedTime, float stopDist)
{
	std::shared_ptr<Actor> owner = GetActor();
	if (!owner) return true;

	DirectX::XMFLOAT3 pos = owner->GetPosition();

	float fdx = worldTarget.x - pos.x;
	float fdz = worldTarget.z - pos.z;
	if (fdx * fdx + fdz * fdz <= stopDist * stopDist)
	{
		path.clear();
		stuckTimer = 0.0f;
		return true;
	}

	float tdx = worldTarget.x - lastPathTarget.x;
	float tdz = worldTarget.z - lastPathTarget.z;
	bool targetMoved = (tdx * tdx + tdz * tdz) > (2.0f * 2.0f);
	repathTimer -= elapsedTime;
	if (repathTimer <= 0.0f || path.empty() || targetMoved)
	{
		repathTimer = repathInterval;
		lastPathTarget = worldTarget;
		pathIndex = 0;
		if (!navGrid || !navGrid->IsBuilt() || !navGrid->FindPath(pos, worldTarget, path))
			path.clear();
	}

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
			const int HALF = 4;
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

	if (groundSnap && !stageCollision.triangles.empty())
	{
		DirectX::XMFLOAT3 rayOrigin = { newPos.x, newPos.y + groundProbeUp, newPos.z };
		DirectX::XMFLOAT3 rayDir = { 0.0f, -1.0f, 0.0f };
		float gd = 0.0f; DirectX::XMFLOAT3 gp, gn;
		if (stageCollision.RayCast(rayOrigin, rayDir, groundProbeUp + groundProbeDown, gd, gp, gn) && gn.y > 0.4f)
			newPos.y = gp.y;
	}

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
			p.x += moveX; p.z += moveZ;
			break;
		}

		float allowed = dist - collisionRadius;
		if (allowed < 0.0f) allowed = 0.0f;
		p.x += dir.x * allowed;
		p.z += dir.z * allowed;

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