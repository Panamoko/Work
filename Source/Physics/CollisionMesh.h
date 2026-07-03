#pragma once

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <DirectXMath.h>

// Static triangle soup accelerated by a uniform spatial-hash grid.
// Owns the geometry queried by the character controller (sphere sweep / ray).
struct CollisionMesh {
	struct Triangle { DirectX::XMFLOAT3 positions[3]; DirectX::XMFLOAT3 normal; };
	std::vector<Triangle> triangles;

	float cellSize = 3.0f;
	std::unordered_map<unsigned long long, std::vector<size_t>> gridCells;

	unsigned long long ComputeHash(int x, int y, int z) const {
		unsigned long long hx = static_cast<unsigned long long>(x & 0x1FFFFF);
		unsigned long long hy = static_cast<unsigned long long>(y & 0x1FFFFF);
		unsigned long long hz = static_cast<unsigned long long>(z & 0x1FFFFF);
		return (hx << 42) | (hy << 21) | hz;
	}

	void BuildSpatialGrid() {
		gridCells.clear();
		for (size_t i = 0; i < triangles.size(); ++i) {
			const auto& tri = triangles[i];
			float minX = (std::min)({ tri.positions[0].x, tri.positions[1].x, tri.positions[2].x });
			float minY = (std::min)({ tri.positions[0].y, tri.positions[1].y, tri.positions[2].y });
			float minZ = (std::min)({ tri.positions[0].z, tri.positions[1].z, tri.positions[2].z });
			float maxX = (std::max)({ tri.positions[0].x, tri.positions[1].x, tri.positions[2].x });
			float maxY = (std::max)({ tri.positions[0].y, tri.positions[1].y, tri.positions[2].y });
			float maxZ = (std::max)({ tri.positions[0].z, tri.positions[1].z, tri.positions[2].z });

			int startX = static_cast<int>(std::floor(minX / cellSize));
			int startY = static_cast<int>(std::floor(minY / cellSize));
			int startZ = static_cast<int>(std::floor(minZ / cellSize));
			int endX = static_cast<int>(std::floor(maxX / cellSize));
			int endY = static_cast<int>(std::floor(maxY / cellSize));
			int endZ = static_cast<int>(std::floor(maxZ / cellSize));

			for (int x = startX; x <= endX; ++x) {
				for (int y = startY; y <= endY; ++y) {
					for (int z = startZ; z <= endZ; ++z) {
						gridCells[ComputeHash(x, y, z)].push_back(i);
					}
				}
			}
		}
	}

	// Sphere sweep against the mesh; keeps the nearest hit within 'distance'.
	bool SphereCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float radius,
		float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal) const;

	// Moller-Trumbore ray cast against the mesh, up to 'maxDistance'.
	bool RayCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance,
		float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal) const;
};