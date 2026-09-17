#include "AI/NavGrid.h"
#include "Physics/CollisionMesh.h"
#include <queue>
#include <cfloat>
#include <cmath>
#include <algorithm>
#include <tracy/Tracy.hpp>

namespace
{
	const int DIRS = 8;
	const int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
	const int DZ[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
}

void NavGrid::Build(const CollisionMesh& m, float cs, float ah, float step, const DirectX::XMFLOAT3& seed)
{
	ZoneScopedN("NavGrid::Build");
	cells.clear();
	cols = rows = 0;
	cellSize = (cs > 0.01f) ? cs : 1.0f;
	agentHeight = ah;
	maxStep = (step > 0.01f) ? step : 0.4f;
	if (m.triangles.empty()) return;

	// Stage bounds.
	float maxX = -FLT_MAX, maxZ = -FLT_MAX, maxY = -FLT_MAX, minY = FLT_MAX;
	minX = minZ = FLT_MAX;
	for (const auto& tri : m.triangles)
		for (int i = 0; i < 3; ++i)
		{
			const auto& p = tri.positions[i];
			minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
			minZ = std::min(minZ, p.z); maxZ = std::max(maxZ, p.z);
			minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
		}

	while (((maxX - minX) / cellSize + 2) * ((maxZ - minZ) / cellSize + 2) > 40000.0f)
		cellSize *= 2.0f;

	cols = (int)std::ceil((maxX - minX) / cellSize) + 1;
	rows = (int)std::ceil((maxZ - minZ) / cellSize) + 1;
	if (cols <= 0 || rows <= 0) { cols = rows = 0; return; }
	cells.assign((size_t)cols * rows, Cell{});

	const float rayTop = maxY + 2.0f;
	const float rayLen = (maxY - minY) + 6.0f;

	// 1) Walkability: down-ray at each cell centre; floor hit (up normal) -> walkable.
	for (int cz = 0; cz < rows; ++cz)
		for (int cx = 0; cx < cols; ++cx)
		{
			DirectX::XMFLOAT3 origin = { minX + (cx + 0.5f) * cellSize, rayTop, minZ + (cz + 0.5f) * cellSize };
			DirectX::XMFLOAT3 dir = { 0.0f, -1.0f, 0.0f };
			float d = 0.0f; DirectX::XMFLOAT3 hp, hn;
			if (m.RayCast(origin, dir, rayLen, d, hp, hn) && hn.y > 0.5f)
			{
				Cell& c = cells[Index(cx, cz)];
				c.walkable = true;
				c.floorY = hp.y;
			}
		}

	// 2) Edges: neighbour walkable, small height step, and no wall between.
	for (int cz = 0; cz < rows; ++cz)
		for (int cx = 0; cx < cols; ++cx)
		{
			Cell& c = cells[Index(cx, cz)];
			if (!c.walkable) continue;

			DirectX::XMFLOAT3 a = { minX + (cx + 0.5f) * cellSize, c.floorY + agentHeight * 0.5f, minZ + (cz + 0.5f) * cellSize };

			for (int k = 0; k < DIRS; ++k)
			{
				int nx = cx + DX[k], nz = cz + DZ[k];
				if (!InBounds(nx, nz)) continue;
				const Cell& n = cells[Index(nx, nz)];
				if (!n.walkable) continue;

				// Height step too big -> this is climbing onto a prop, not walking.
				if (std::fabs(n.floorY - c.floorY) > maxStep) continue;

				// No diagonal corner cutting.
				if (DX[k] != 0 && DZ[k] != 0)
				{
					if (!cells[Index(cx + DX[k], cz)].walkable) continue;
					if (!cells[Index(cx, cz + DZ[k])].walkable) continue;
				}

				DirectX::XMFLOAT3 b = { minX + (nx + 0.5f) * cellSize, n.floorY + agentHeight * 0.5f, minZ + (nz + 0.5f) * cellSize };
				DirectX::XMFLOAT3 seg = { b.x - a.x, b.y - a.y, b.z - a.z };
				float dist = std::sqrt(seg.x * seg.x + seg.y * seg.y + seg.z * seg.z);
				float hd = 0.0f; DirectX::XMFLOAT3 hp, hn;
				if (dist > 1e-4f && m.RayCast(a, seg, dist, hd, hp, hn) && hd < dist - 0.05f)
					continue; // wall between

				c.edges |= (unsigned char)(1 << k);
			}
		}

	// 3) Reachability: flood-fill from the seed cell over the edges.
	int scx, scz;
	if (WorldToCell(seed, scx, scz))
	{
		int s = cells[Index(scx, scz)].walkable ? Index(scx, scz) : NearestWalkable(scx, scz);
		if (s >= 0) FloodFill(s);
	}
	else
	{
		// Seed outside the grid: treat every walkable cell as reachable.
		for (Cell& c : cells) if (c.walkable) c.reachable = true;
	}
}

void NavGrid::FloodFill(int seedIdx)
{
	std::queue<int> q;
	cells[seedIdx].reachable = true;
	q.push(seedIdx);

	while (!q.empty())
	{
		int cur = q.front(); q.pop();
		int cx = cur % cols, cz = cur / cols;
		unsigned char edges = cells[cur].edges;
		for (int k = 0; k < DIRS; ++k)
		{
			if (!(edges & (1 << k))) continue;
			int nIdx = Index(cx + DX[k], cz + DZ[k]);
			if (cells[nIdx].reachable || !cells[nIdx].walkable) continue;
			cells[nIdx].reachable = true;
			q.push(nIdx);
		}
	}
}

bool NavGrid::WorldToCell(const DirectX::XMFLOAT3& w, int& cx, int& cz) const
{
	if (cells.empty()) return false;
	cx = (int)std::floor((w.x - minX) / cellSize);
	cz = (int)std::floor((w.z - minZ) / cellSize);
	return InBounds(cx, cz);
}

int NavGrid::NearestWalkable(int cx, int cz) const
{
	if (InBounds(cx, cz) && cells[Index(cx, cz)].walkable) return Index(cx, cz);
	int maxR = std::max(cols, rows);
	for (int r = 1; r < maxR; ++r)
		for (int dz = -r; dz <= r; ++dz)
			for (int dx = -r; dx <= r; ++dx)
			{
				if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
				int nx = cx + dx, nz = cz + dz;
				if (InBounds(nx, nz) && cells[Index(nx, nz)].walkable) return Index(nx, nz);
			}
	return -1;
}

int NavGrid::NearestReachable(int cx, int cz) const
{
	if (InBounds(cx, cz) && cells[Index(cx, cz)].reachable) return Index(cx, cz);
	int maxR = std::max(cols, rows);
	for (int r = 1; r < maxR; ++r)
		for (int dz = -r; dz <= r; ++dz)
			for (int dx = -r; dx <= r; ++dx)
			{
				if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
				int nx = cx + dx, nz = cz + dz;
				if (InBounds(nx, nz) && cells[Index(nx, nz)].reachable) return Index(nx, nz);
			}
	return -1;
}

int NavGrid::ReachableCount() const
{
	int n = 0;
	for (const Cell& c : cells) if (c.reachable) ++n;
	return n;
}

bool NavGrid::IsWalkable(const DirectX::XMFLOAT3& w) const
{
	int cx, cz;
	if (!WorldToCell(w, cx, cz)) return false;
	return cells[Index(cx, cz)].reachable;
}

bool NavGrid::FindPath(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& goal,
	std::vector<DirectX::XMFLOAT3>& outPath) const
{
	ZoneScopedN("NavGrid::FindPath");
	outPath.clear();
	if (cells.empty()) return false;

	int sx, sz, gx, gz;
	if (!WorldToCell(start, sx, sz) || !WorldToCell(goal, gx, gz)) return false;

	int startIdx = cells[Index(sx, sz)].reachable ? Index(sx, sz) : NearestReachable(sx, sz);
	int goalIdx = cells[Index(gx, gz)].reachable ? Index(gx, gz) : NearestReachable(gx, gz);
	if (startIdx < 0 || goalIdx < 0) return false;

	const int n = (int)cells.size();
	// スクラッチは使い回し（毎リパスの n×3 アロケーションを排除）。
	static thread_local std::vector<float> g;
	static thread_local std::vector<int>   came;
	static thread_local std::vector<char>  closed;
	g.assign(n, FLT_MAX);
	came.assign(n, -1);
	closed.assign(n, 0);

	int goalX = goalIdx % cols, goalZ = goalIdx / cols;
	auto heur = [&](int idx)
		{
			float ddx = (float)(idx % cols - goalX);
			float ddz = (float)(idx / cols - goalZ);
			return std::sqrt(ddx * ddx + ddz * ddz) * cellSize;
		};

	typedef std::pair<float, int> PQ;
	std::priority_queue<PQ, std::vector<PQ>, std::greater<PQ>> open;
	g[startIdx] = 0.0f;
	open.push({ heur(startIdx), startIdx });

	while (!open.empty())
	{
		int cur = open.top().second; open.pop();
		if (cur == goalIdx) break;
		if (closed[cur]) continue;
		closed[cur] = 1;

		int cx = cur % cols, cz = cur / cols;
		unsigned char edges = cells[cur].edges;
		for (int k = 0; k < DIRS; ++k)
		{
			if (!(edges & (1 << k))) continue;
			int nIdx = Index(cx + DX[k], cz + DZ[k]);
			if (closed[nIdx]) continue;
			float stepCost = (DX[k] != 0 && DZ[k] != 0) ? 1.41421356f : 1.0f;
			float ng = g[cur] + stepCost * cellSize;
			if (ng < g[nIdx])
			{
				g[nIdx] = ng;
				came[nIdx] = cur;
				open.push({ ng + heur(nIdx), nIdx });
			}
		}
	}

	if (goalIdx != startIdx && came[goalIdx] < 0) return false;

	// --- reconstruct cell-index path (start -> goal) ---
	std::vector<int> idxPath;
	for (int cur = goalIdx; cur != -1; cur = came[cur])
	{
		idxPath.push_back(cur);
		if (cur == startIdx) break;
	}
	std::reverse(idxPath.begin(), idxPath.end());

	// --- string-pulling: 見通しが通る中間点を省いて直線化（8方向グリッドの
	//     ジグザグ遠回りを解消）。壁の角越えは LineOfSight 側で禁止。 ---
	std::vector<int> smoothed;
	if (!idxPath.empty())
	{
		smoothed.push_back(idxPath[0]);
		int anchor = 0;
		for (int i = 2; i < (int)idxPath.size(); ++i)
		{
			if (!LineOfSight(idxPath[anchor], idxPath[i]))
			{
				smoothed.push_back(idxPath[i - 1]);
				anchor = i - 1;
			}
		}
		if (idxPath.size() >= 2) smoothed.push_back(idxPath.back());
	}

	// --- convert kept cells to world centers ---
	outPath.reserve(smoothed.size());
	for (int idx : smoothed)
	{
		int cx = idx % cols, cz = idx / cols;
		outPath.push_back({ minX + (cx + 0.5f) * cellSize, cells[idx].floorY, minZ + (cz + 0.5f) * cellSize });
	}
	return !outPath.empty();
}

void NavGrid::CollectDebugCells(const DirectX::XMFLOAT3& center, float radius,
	std::vector<DirectX::XMFLOAT3>& reachableTris,
	std::vector<DirectX::XMFLOAT3>& isolatedTris, float yLift) const
{
	if (cells.empty()) return;

	int rcell = (int)std::ceil(radius / cellSize);
	int cx0 = (int)std::floor((center.x - minX) / cellSize);
	int cz0 = (int)std::floor((center.z - minZ) / cellSize);
	float half = cellSize * 0.46f;

	for (int cz = cz0 - rcell; cz <= cz0 + rcell; ++cz)
		for (int cx = cx0 - rcell; cx <= cx0 + rcell; ++cx)
		{
			if (!InBounds(cx, cz)) continue;
			const Cell& c = cells[Index(cx, cz)];
			if (!c.walkable) continue; // only draw real floor -> hugs the stage

			float wx = minX + (cx + 0.5f) * cellSize;
			float wz = minZ + (cz + 0.5f) * cellSize;
			float y = c.floorY + yLift;

			DirectX::XMFLOAT3 a = { wx - half, y, wz - half };
			DirectX::XMFLOAT3 b = { wx + half, y, wz - half };
			DirectX::XMFLOAT3 d = { wx - half, y, wz + half };
			DirectX::XMFLOAT3 e = { wx + half, y, wz + half };

			std::vector<DirectX::XMFLOAT3>& out = c.reachable ? reachableTris : isolatedTris;
			out.push_back(a); out.push_back(b); out.push_back(e);
			out.push_back(a); out.push_back(e); out.push_back(d);
		}
}

// True if a straight line between two cells stays entirely on reachable cells
// (diagonal corner-cutting disallowed). Used to straighten the A* path.
bool NavGrid::LineOfSight(int aIdx, int bIdx) const
{
	int ax = aIdx % cols, az = aIdx / cols;
	int bx = bIdx % cols, bz = bIdx / cols;

	int dx = (bx >= ax) ? (bx - ax) : (ax - bx);
	int dz = (bz >= az) ? (bz - az) : (az - bz);
	int sx = (ax < bx) ? 1 : -1;
	int sz = (az < bz) ? 1 : -1;
	int err = dx - dz;
	int x = ax, z = az;

	while (true)
	{
		if (!InBounds(x, z) || !cells[Index(x, z)].reachable) return false;
		if (x == bx && z == bz) break;

		int e2 = 2 * err;
		bool stepX = (e2 > -dz);
		bool stepZ = (e2 < dx);
		if (stepX && stepZ)
		{
			// diagonal step: both orthogonal neighbors must be open (no corner cut)
			if (!InBounds(x + sx, z) || !cells[Index(x + sx, z)].reachable) return false;
			if (!InBounds(x, z + sz) || !cells[Index(x, z + sz)].reachable) return false;
			err -= dz; x += sx;
			err += dx; z += sz;
		}
		else if (stepX) { err -= dz; x += sx; }
		else { err += dx; z += sz; }
	}
	return true;
}