#pragma once

#include <vector>

#include <DirectXMath.h>

struct CollisionMesh;

// Walkable grid built from the stage collision.
//  - walkable  : a downward ray finds floor (up-facing normal)
//  - edges     : neighbour is walkable, small height step, and no wall between
//  - reachable : flood-filled from a seed point, so isolated prop-tops / other
//                rooms are excluded. Pathfinding and patrol use reachable cells.
class NavGrid
{
public:
	void Build(const CollisionMesh& mesh, float cellSize, float agentHeight,
		float maxStep, const DirectX::XMFLOAT3& seed);

	bool IsBuilt() const { return !cells.empty(); }

	bool FindPath(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& goal,
		std::vector<DirectX::XMFLOAT3>& outPath) const;

	bool IsWalkable(const DirectX::XMFLOAT3& world) const; // reachable ground

	// Debug: cell quads (2 tris each) near 'center'.
	//   reachable ground -> reachableTris, walkable-but-isolated -> isolatedTris.
	void CollectDebugCells(const DirectX::XMFLOAT3& center, float radius,
		std::vector<DirectX::XMFLOAT3>& reachableTris,
		std::vector<DirectX::XMFLOAT3>& isolatedTris,
		float yLift) const;

	float GetCellSize()     const { return cellSize; }
	int   CellCount()       const { return (int)cells.size(); }
	int   ReachableCount()  const;

private:
	struct Cell
	{
		bool  walkable = false;
		bool  reachable = false;
		float floorY = 0.0f;
		unsigned char edges = 0;
	};

	int  Index(int cx, int cz) const { return cz * cols + cx; }
	bool InBounds(int cx, int cz) const { return cx >= 0 && cx < cols && cz >= 0 && cz < rows; }
	bool WorldToCell(const DirectX::XMFLOAT3& w, int& cx, int& cz) const;
	int  NearestWalkable(int cx, int cz) const;
	int  NearestReachable(int cx, int cz) const;
	void FloodFill(int seedIdx);
	bool LineOfSight(int aIdx, int bIdx) const;


	int   cols = 0, rows = 0;
	float cellSize = 1.0f;
	float agentHeight = 1.0f;
	float maxStep = 0.4f;
	float minX = 0.0f, minZ = 0.0f;
	std::vector<Cell> cells;
};