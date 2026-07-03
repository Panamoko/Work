#pragma once

#include <vector>
#include <DirectXMath.h>

struct CollisionMesh;

// Uniform walkable grid built from the stage collision, with A* pathfinding.
// Walkability = a downward ray finds floor; edges = a ray between two cell
// centers is not blocked by a wall (both reuse CollisionMesh::RayCast).
class NavGrid
{
public:
	void Build(const CollisionMesh& mesh, float cellSize, float agentHeight);
	bool IsBuilt() const { return !cells.empty(); }

	// A*: fills outPath with world-space waypoints (start cell -> goal cell).
	bool FindPath(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& goal,
		std::vector<DirectX::XMFLOAT3>& outPath) const;

	bool IsWalkable(const DirectX::XMFLOAT3& world) const;

private:
	struct Cell { bool walkable = false; float floorY = 0.0f; unsigned char edges = 0; };

	int  Index(int cx, int cz) const { return cz * cols + cx; }
	bool InBounds(int cx, int cz) const { return cx >= 0 && cx < cols && cz >= 0 && cz < rows; }
	bool WorldToCell(const DirectX::XMFLOAT3& w, int& cx, int& cz) const;
	int  NearestWalkable(int cx, int cz) const; // cell index, or -1

	int   cols = 0, rows = 0;
	float cellSize = 1.0f;
	float agentHeight = 1.0f;
	float minX = 0.0f, minZ = 0.0f;
	std::vector<Cell> cells;
};