#include "Physics/CollisionMesh.h"
#include "Physics/SphereCast.h"

bool CollisionMesh::SphereCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float radius, float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal) const {
	bool hit = false;
	DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&origin), Direction = DirectX::XMLoadFloat3(&direction);
	DirectX::XMVECTOR End = DirectX::XMVectorAdd(Start, DirectX::XMVectorScale(Direction, distance));

	float minX = (std::min)(origin.x, origin.x + direction.x * distance) - radius;
	float minY = (std::min)(origin.y, origin.y + direction.y * distance) - radius;
	float minZ = (std::min)(origin.z, origin.z + direction.z * distance) - radius;
	float maxX = (std::max)(origin.x, origin.x + direction.x * distance) + radius;
	float maxY = (std::max)(origin.y, origin.y + direction.y * distance) + radius;
	float maxZ = (std::max)(origin.z, origin.z + direction.z * distance) + radius;

	int startX = static_cast<int>(std::floor(minX / cellSize));
	int startY = static_cast<int>(std::floor(minY / cellSize));
	int startZ = static_cast<int>(std::floor(minZ / cellSize));
	int endX = static_cast<int>(std::floor(maxX / cellSize));
	int endY = static_cast<int>(std::floor(maxY / cellSize));
	int endZ = static_cast<int>(std::floor(maxZ / cellSize));

	std::vector<size_t> candidateIndices;
	for (int x = startX; x <= endX; ++x) {
		for (int y = startY; y <= endY; ++y) {
			for (int z = startZ; z <= endZ; ++z) {
				auto it = gridCells.find(ComputeHash(x, y, z));
				if (it != gridCells.end()) {
					candidateIndices.insert(candidateIndices.end(), it->second.begin(), it->second.end());
				}
			}
		}
	}

	std::sort(candidateIndices.begin(), candidateIndices.end());
	candidateIndices.erase(std::unique(candidateIndices.begin(), candidateIndices.end()), candidateIndices.end());

	for (size_t idx : candidateIndices) {
		const auto& triangle = triangles[idx];
		DirectX::XMVECTOR Positions[3] = { DirectX::XMLoadFloat3(&triangle.positions[0]), DirectX::XMLoadFloat3(&triangle.positions[1]), DirectX::XMLoadFloat3(&triangle.positions[2]) };
		SphereCastResult result;
		if (IntersectSphereCastVsTriangle(Start, End, radius, Positions, &result)) {
			if (distance < result.distance) continue;
			distance = result.distance;
			DirectX::XMStoreFloat3(&hitPosition, DirectX::XMVectorAdd(Start, DirectX::XMVectorScale(Direction, distance)));
			DirectX::XMStoreFloat3(&hitNormal, result.normal);
			hit = true;
		}
	}
	return hit;
}

bool CollisionMesh::RayCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance,
	float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal) const {
	bool hit = false;
	distance = maxDistance;

	DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&origin);
	DirectX::XMVECTOR Dir = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&direction));
	DirectX::XMFLOAT3 d; DirectX::XMStoreFloat3(&d, Dir);

	const float BIG = 1.0e30f; // FLT_MAX 代わり（追加includeを避けるため）

	// レイの始点セル
	int cx = static_cast<int>(std::floor(origin.x / cellSize));
	int cy = static_cast<int>(std::floor(origin.y / cellSize));
	int cz = static_cast<int>(std::floor(origin.z / cellSize));

	// 各軸：進む向き step / 次のセル境界までの距離 tMax / 1セル進むごとの距離 tDelta
	auto setupAxis = [&](float o, float dir, int cell, int& step, float& tMax, float& tDelta) {
		if (dir > 0.0f) {
			step = 1;
			tMax = ((cell + 1) * cellSize - o) / dir;
			tDelta = cellSize / dir;
		}
		else if (dir < 0.0f) {
			step = -1;
			tMax = (cell * cellSize - o) / dir;
			tDelta = cellSize / -dir;
		}
		else {
			step = 0; tMax = BIG; tDelta = BIG;
		}
		};

	int stepX, stepY, stepZ;
	float tMaxX, tMaxY, tMaxZ, tDeltaX, tDeltaY, tDeltaZ;
	setupAxis(origin.x, d.x, cx, stepX, tMaxX, tDeltaX);
	setupAxis(origin.y, d.y, cy, stepY, tMaxY, tDeltaY);
	setupAxis(origin.z, d.z, cz, stepZ, tMaxZ, tDeltaZ);

	if (stepX == 0 && stepY == 0 && stepZ == 0) return false; // 方向ゼロ

	const float EPSILON = 0.0000001f;
	float cellEntryT = 0.0f;

	while (cellEntryT <= distance) {
		auto it = gridCells.find(ComputeHash(cx, cy, cz));
		if (it != gridCells.end()) {
			for (size_t idx : it->second) {
				const auto& triangle = triangles[idx];
				DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&triangle.positions[0]);
				DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&triangle.positions[1]);
				DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&triangle.positions[2]);

				DirectX::XMVECTOR edge1 = DirectX::XMVectorSubtract(v1, v0);
				DirectX::XMVECTOR edge2 = DirectX::XMVectorSubtract(v2, v0);
				DirectX::XMVECTOR h = DirectX::XMVector3Cross(Dir, edge2);
				float a = DirectX::XMVectorGetX(DirectX::XMVector3Dot(edge1, h));
				if (a > -EPSILON && a < EPSILON) continue;

				float f = 1.0f / a;
				DirectX::XMVECTOR s = DirectX::XMVectorSubtract(Start, v0);
				float u = f * DirectX::XMVectorGetX(DirectX::XMVector3Dot(s, h));
				if (u < 0.0f || u > 1.0f) continue;

				DirectX::XMVECTOR q = DirectX::XMVector3Cross(s, edge1);
				float v = f * DirectX::XMVectorGetX(DirectX::XMVector3Dot(Dir, q));
				if (v < 0.0f || u + v > 1.0f) continue;

				float t = f * DirectX::XMVectorGetX(DirectX::XMVector3Dot(edge2, q));
				if (t > EPSILON && t < distance) {
					distance = t;
					DirectX::XMStoreFloat3(&hitNormal, DirectX::XMVector3Normalize(DirectX::XMVector3Cross(edge1, edge2)));
					hit = true;
				}
			}
		}

		// 次セルの入口までの距離
		float nextT = (std::min)(tMaxX, (std::min)(tMaxY, tMaxZ));

		// すでに見つけたヒットが、これ以降のセルより手前なら確定して打ち切り
		if (hit && distance <= nextT) break;

		// レイに沿って次のセルへ 1 歩進める
		cellEntryT = nextT;
		if (tMaxX <= tMaxY && tMaxX <= tMaxZ) { cx += stepX; tMaxX += tDeltaX; }
		else if (tMaxY <= tMaxZ) { cy += stepY; tMaxY += tDeltaY; }
		else { cz += stepZ; tMaxZ += tDeltaZ; }
	}

	if (hit) {
		DirectX::XMStoreFloat3(&hitPosition, DirectX::XMVectorAdd(Start, DirectX::XMVectorScale(Dir, distance)));
	}
	return hit;
}