#include "AI/NavGrid.h"
#include "Physics/CollisionMesh.h"
#include <queue>
#include <cfloat>
#include <cmath>
#include <algorithm>

namespace
{
	const int DIRS = 8;
	const int DX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
	const int DZ[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };
}

void NavGrid::Build(const CollisionMesh& m, float cs, float ah, float refY, float floorTol)
{
	cells.clear();
	cols = rows = 0;
	cellSize = (cs > 0.01f) ? cs : 1.0f;
	agentHeight = ah;
	if (m.triangles.empty()) return;

	// Stage XZ bounds.
	float maxX = -FLT_MAX, maxZ = -FLT_MAX;
	minX = minZ = FLT_MAX;
	for (const auto& tri : m.triangles)
		for (int i = 0; i < 3; ++i)
		{
			const auto& p = tri.positions[i];
			minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
			minZ = std::min(minZ, p.z); maxZ = std::max(maxZ, p.z);
		}

	// Keep the grid from getting huge (raise cell size if needed).
	while (((maxX - minX) / cellSize + 2) * ((maxZ - minZ) / cellSize + 2) > 200000.0f)
		cellSize *= 2.0f;

	cols = (int)std::ceil((maxX - minX) / cellSize) + 1;
	rows = (int)std::ceil((maxZ - minZ) / cellSize) + 1;
	if (cols <= 0 || rows <= 0) { cols = rows = 0; return; }
	cells.assign((size_t)cols * rows, Cell{});

	// Cast DOWN from just above head-height *at ground level*, and accept the hit
	// only if it lands near refY. This ignores prop tops (tires/crates) and
	// catwalks above the ground, so the mesh follows the real floor.
	const float castTop = refY + agentHeight + 1.0f;
	const float rayLen = (agentHeight + 1.0f) + floorTol + 0.5f;

	for (int cz = 0; cz < rows; ++cz)
		for (int cx = 0; cx < cols; ++cx)
		{
			DirectX::XMFLOAT3 origin = { minX + (cx + 0.5f) * cellSize, castTop, minZ + (cz + 0.5f) * cellSize };
			DirectX::XMFLOAT3 dir = { 0.0f, -1.0f, 0.0f };
			float d = 0.0f; DirectX::XMFLOAT3 hp, hn;
			if (m.RayCast(origin, dir, rayLen, d, hp, hn) && hn.y > 0.5f && fabsf(hp.y - refY) <= floorTol)
			{
				Cell& c = cells[Index(cx, cz)];
				c.walkable = true;
				c.floorY = hp.y;
			}
		}

	// Edges: connect walkable neighbours if the segment between them is wall-free.
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

				if (DX[k] != 0 && DZ[k] != 0)
				{
					if (!cells[Index(cx + DX[k], cz)].walkable) continue;
					if (!cells[Index(cx, cz + DZ[k])].walkable) continue;
				}

				DirectX::XMFLOAT3 b = { minX + (nx + 0.5f) * cellSize, n.floorY + agentHeight * 0.5f, minZ + (nz + 0.5f) * cellSize };
				DirectX::XMFLOAT3 seg = { b.x - a.x, b.y - a.y, b.z - a.z };
				float dist = std::sqrt(seg.x * seg.x + seg.y * seg.y + seg.z * seg.z);
				float hitDist = 0.0f; DirectX::XMFLOAT3 hp, hn;
				if (dist > 1e-4f && m.RayCast(a, seg, dist, hitDist, hp, hn) && hitDist < dist - 0.05f)
					continue;

				c.edges |= (unsigned char)(1 << k);
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
				if (std::max(std::abs(dx), std::abs(dz)) != r) continue; // ring only
				int nx = cx + dx, nz = cz + dz;
				if (InBounds(nx, nz) && cells[Index(nx, nz)].walkable) return Index(nx, nz);
			}
	return -1;
}

bool NavGrid::IsWalkable(const DirectX::XMFLOAT3& w) const
{
	int cx, cz;
	if (!WorldToCell(w, cx, cz)) return false;
	return cells[Index(cx, cz)].walkable;
}

bool NavGrid::FindPath(const DirectX::XMFLOAT3& start, const DirectX::XMFLOAT3& goal,
	std::vector<DirectX::XMFLOAT3>& outPath) const
{
	outPath.clear();
	if (cells.empty()) return false;

	int sx, sz, gx, gz;
	if (!WorldToCell(start, sx, sz) || !WorldToCell(goal, gx, gz)) return false;

	int startIdx = (cells[Index(sx, sz)].walkable) ? Index(sx, sz) : NearestWalkable(sx, sz);
	int goalIdx = (cells[Index(gx, gz)].walkable) ? Index(gx, gz) : NearestWalkable(gx, gz);
	if (startIdx < 0 || goalIdx < 0) return false;

	const int n = (int)cells.size();
	std::vector<float> g(n, FLT_MAX);
	std::vector<int> came(n, -1);
	std::vector<char> closed(n, 0);

	int goalX = goalIdx % cols, goalZ = goalIdx / cols;
	auto heur = [&](int idx)
		{
			float ddx = (float)(idx % cols - goalX);
			float ddz = (float)(idx / cols - goalZ);
			return std::sqrt(ddx * ddx + ddz * ddz) * cellSize;
		};

	typedef std::pair<float, int> PQ; // f, idx
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
			float step = (DX[k] != 0 && DZ[k] != 0) ? 1.41421356f : 1.0f;
			float ng = g[cur] + step * cellSize;
			if (ng < g[nIdx])
			{
				g[nIdx] = ng;
				came[nIdx] = cur;
				open.push({ ng + heur(nIdx), nIdx });
			}
		}
	}

	if (goalIdx != startIdx && came[goalIdx] < 0) return false;

	std::vector<DirectX::XMFLOAT3> rev;
	for (int cur = goalIdx; cur != -1; cur = came[cur])
	{
		int cx = cur % cols, cz = cur / cols;
		rev.push_back({ minX + (cx + 0.5f) * cellSize, cells[cur].floorY, minZ + (cz + 0.5f) * cellSize });
		if (cur == startIdx) break;
	}
	std::reverse(rev.begin(), rev.end());
	outPath = std::move(rev);
	return !outPath.empty();
}

void NavGrid::CollectDebugCells(const DirectX::XMFLOAT3& center, float radius,
	std::vector<DirectX::XMFLOAT3>& walkTris,
	std::vector<DirectX::XMFLOAT3>& blockTris, float yLift) const
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

			float wx = minX + (cx + 0.5f) * cellSize;
			float wz = minZ + (cz + 0.5f) * cellSize;
			float y = (c.walkable ? c.floorY : center.y) + yLift;

			DirectX::XMFLOAT3 a = { wx - half, y, wz - half };
			DirectX::XMFLOAT3 b = { wx + half, y, wz - half };
			DirectX::XMFLOAT3 d = { wx - half, y, wz + half };
			DirectX::XMFLOAT3 e = { wx + half, y, wz + half };

			std::vector<DirectX::XMFLOAT3>& out = c.walkable ? walkTris : blockTris;
			out.push_back(a); out.push_back(b); out.push_back(e);
			out.push_back(a); out.push_back(e); out.push_back(d);
		}
}