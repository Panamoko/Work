#include "Engine/Physics/MeshBVH.h"
#include "Engine/Graphics/Mesh.h"
#include <algorithm>
#include <cfloat>
#include <cmath>

using namespace DirectX;

namespace uel {
    namespace {

        constexpr uint32_t kLeafSize = 4;       // 葉に入れる三角形の数の上限
        constexpr float    kEps = 1e-6f;        // 線と三角形が平行かどうかの境目（今までの RaycastMesh と同じ値）
        constexpr int      kStackSize = 64;     // 調べるときの箱の積み上げ（木の深さは 72 万個でも 20 ほど）

        // 作るときだけ使う、三角形ごとの箱と真ん中
        struct BuildCtx {
            const XMFLOAT3* bmin;
            const XMFLOAT3* bmax;
            const XMFLOAT3* center;
            uint32_t* ids;
        };

        float Axis(const XMFLOAT3& v, int a) { return a == 0 ? v.x : (a == 1 ? v.y : v.z); }

        // 線（o + t * d）を、箱の中を通る t の範囲 [t0, t1] に絞る。通らなければ false。
        // d の成分が 0 のときは、その向きの板の間に o があるかだけを見る（0 で割らない）
        bool ClipToBox(const float o[3], const float invD[3], const bool flat[3],
            const float bmin[3], const float bmax[3], float& t0, float& t1)
        {
            for (int k = 0; k < 3; ++k) {
                if (flat[k]) {
                    if (o[k] < bmin[k] || o[k] > bmax[k]) return false;
                    continue;
                }
                float ta = (bmin[k] - o[k]) * invD[k];
                float tb = (bmax[k] - o[k]) * invD[k];
                if (ta > tb) std::swap(ta, tb);
                if (ta > t0) t0 = ta;
                if (tb < t1) t1 = tb;
                if (t0 > t1) return false;
            }
            return true;
        }

        // 線と三角形の交わり（Möller–Trumbore。今までの RaycastMesh と同じ式）。t の範囲は呼ぶ側で見る
        bool LineTriangle(const XMFLOAT3& o, const XMFLOAT3& d,
            const XMFLOAT3& v0, const XMFLOAT3& v1, const XMFLOAT3& v2, float& t)
        {
            const float e1x = v1.x - v0.x, e1y = v1.y - v0.y, e1z = v1.z - v0.z;
            const float e2x = v2.x - v0.x, e2y = v2.y - v0.y, e2z = v2.z - v0.z;
            const float hx = d.y * e2z - d.z * e2y, hy = d.z * e2x - d.x * e2z, hz = d.x * e2y - d.y * e2x;
            const float a = e1x * hx + e1y * hy + e1z * hz;
            if (a > -kEps && a < kEps) return false;            // 線が三角形の面と平行
            const float f = 1.0f / a;
            const float sx = o.x - v0.x, sy = o.y - v0.y, sz = o.z - v0.z;
            const float u = f * (sx * hx + sy * hy + sz * hz);
            if (u < 0.0f || u > 1.0f) return false;
            const float qx = sy * e1z - sz * e1y, qy = sz * e1x - sx * e1z, qz = sx * e1y - sy * e1x;
            const float v = f * (d.x * qx + d.y * qy + d.z * qz);
            if (v < 0.0f || u + v > 1.0f) return false;
            t = f * (e2x * qx + e2y * qy + e2z * qz);
            return true;
        }

        void Prepare(const XMFLOAT3& d, float invD[3], bool flat[3])
        {
            const float dd[3] = { d.x, d.y, d.z };
            for (int k = 0; k < 3; ++k) {
                flat[k] = std::fabs(dd[k]) < 1e-12f;
                invD[k] = flat[k] ? 0.0f : 1.0f / dd[k];
            }
        }
    } // namespace

    bool MeshBVH::IsBuiltFor(const Mesh& mesh) const
    {
        return builtPositions == mesh.Positions().data() && builtIndexCount == mesh.Indices().size();
    }

    // 作り方：三角形の真ん中の位置で、いちばん長い向きを半分に分けていく（数の真ん中で分ける＝木の深さがそろう）。
    // 真ん中で分けるのに nth_element（全部を並べ替えないので速い）。72 万個で Release 約 0.3 秒の見込み
    void MeshBVH::Build(const Mesh& mesh)
    {
        const auto& pos = mesh.Positions();
        const auto& idx = mesh.Indices();
        const uint32_t n = static_cast<uint32_t>(idx.size() / 3);
        nodes.clear();
        triIds.resize(n);
        builtPositions = pos.data();
        builtIndexCount = idx.size();
        triSubmesh.clear();
        if (n == 0) return;

        std::vector<XMFLOAT3> tmin(n), tmax(n), center(n);
        for (uint32_t i = 0; i < n; ++i) {
            const XMFLOAT3& a = pos[idx[i * 3 + 0]];
            const XMFLOAT3& b = pos[idx[i * 3 + 1]];
            const XMFLOAT3& c = pos[idx[i * 3 + 2]];
            tmin[i] = { (std::min)({ a.x, b.x, c.x }), (std::min)({ a.y, b.y, c.y }), (std::min)({ a.z, b.z, c.z }) };
            tmax[i] = { (std::max)({ a.x, b.x, c.x }), (std::max)({ a.y, b.y, c.y }), (std::max)({ a.z, b.z, c.z }) };
            center[i] = { (tmin[i].x + tmax[i].x) * 0.5f, (tmin[i].y + tmax[i].y) * 0.5f, (tmin[i].z + tmax[i].z) * 0.5f };
            triIds[i] = i;
        }
        // 三角形 → サブメッシュの番号（5-4b）。サブメッシュは添字の範囲（indexOffset から indexCount 個）で決まる
        triSubmesh.assign(n, kNoSubmesh);
        {
            const auto& subs = mesh.GetSubmeshes();
            for (size_t s = 0; s < subs.size(); ++s) {
                const uint32_t first = subs[s].indexOffset / 3;
                const uint32_t last = (std::min)(n, (subs[s].indexOffset + subs[s].indexCount) / 3);
                for (uint32_t t = first; t < last; ++t) triSubmesh[t] = static_cast<uint32_t>(s);
            }
        }
        nodes.reserve(static_cast<size_t>(n / kLeafSize) * 2 + 2);
        BuildCtx ctx{ tmin.data(), tmax.data(), center.data(), triIds.data() };

        // 範囲 [begin, end) の三角形で箱を1つ作り、多ければ2つに分けて子を作る（深さは 20 ほどなので再帰でよい）
        struct Builder {
            std::vector<Node>& nodes;
            BuildCtx& c;
            uint32_t Make(uint32_t begin, uint32_t end) {
                const uint32_t me = static_cast<uint32_t>(nodes.size());
                nodes.push_back(Node{});
                float bmin[3] = { FLT_MAX, FLT_MAX, FLT_MAX }, bmax[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                float cmin[3] = { FLT_MAX, FLT_MAX, FLT_MAX }, cmax[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                for (uint32_t i = begin; i < end; ++i) {
                    const uint32_t t = c.ids[i];
                    for (int k = 0; k < 3; ++k) {
                        bmin[k] = (std::min)(bmin[k], Axis(c.bmin[t], k));
                        bmax[k] = (std::max)(bmax[k], Axis(c.bmax[t], k));
                        const float cc = Axis(c.center[t], k);
                        cmin[k] = (std::min)(cmin[k], cc);
                        cmax[k] = (std::max)(cmax[k], cc);
                    }
                }
                for (int k = 0; k < 3; ++k) { nodes[me].bmin[k] = bmin[k]; nodes[me].bmax[k] = bmax[k]; }

                const uint32_t count = end - begin;
                int axis = 0;
                for (int k = 1; k < 3; ++k) if (cmax[k] - cmin[k] > cmax[axis] - cmin[axis]) axis = k;
                if (count <= kLeafSize || cmax[axis] - cmin[axis] <= 1e-9f) {   // 少ない、または全部同じ所にある：葉
                    nodes[me].start = begin;
                    nodes[me].count = count;
                    return me;
                }
                const uint32_t mid = begin + count / 2;
                const XMFLOAT3* cen = c.center;
                std::nth_element(c.ids + begin, c.ids + mid, c.ids + end,
                    [cen, axis](uint32_t a, uint32_t b) { return Axis(cen[a], axis) < Axis(cen[b], axis); });
                Make(begin, mid);                           // 左の子は me + 1
                const uint32_t right = Make(mid, end);
                nodes[me].start = right;                    // push_back で動くので、番号で書き直す
                nodes[me].count = 0;
                return me;
            }
        };
        Builder{ nodes, ctx }.Make(0, n);
    }

    bool MeshBVH::RayClosest(const Mesh& mesh, const XMFLOAT3& o, const XMFLOAT3& d,
        float tMin, float tMax, float& outT, SubmeshSkip skip) const
    {
        if (nodes.empty()) return false;
        const auto& pos = mesh.Positions();
        const auto& idx = mesh.Indices();
        const float of[3] = { o.x, o.y, o.z };
        float invD[3]; bool flat[3];
        Prepare(d, invD, flat);

        float best = tMax;
        bool found = false;
        uint32_t stack[kStackSize];
        int sp = 0;
        stack[sp++] = 0;
        while (sp > 0) {
            const Node& nd = nodes[stack[--sp]];
            float t0 = tMin, t1 = best;
            if (!ClipToBox(of, invD, flat, nd.bmin, nd.bmax, t0, t1)) continue;   // 今いちばん近い当たりより奥の箱も見ない
            if (nd.count > 0) {
                for (uint32_t i = nd.start; i < nd.start + nd.count; ++i) {
                    const uint32_t tri = triIds[i];
                    if (Skipped(tri, skip)) continue;                  // 当たり判定なしのサブメッシュ（5-4b）
                    float t;
                    if (LineTriangle(o, d, pos[idx[tri * 3]], pos[idx[tri * 3 + 1]], pos[idx[tri * 3 + 2]], t)
                        && t > tMin && t < best) {
                        best = t; found = true;
                    }
                }
            }
            else if (sp + 2 <= kStackSize) {
                // 手前の子を先に見る（先に近い当たりが見つかると、奥の箱を見ずに済む）。後に積んだほうが先に出る
                const uint32_t left = static_cast<uint32_t>(&nd - nodes.data()) + 1, right = nd.start;
                float la0 = tMin, la1 = best, ra0 = tMin, ra1 = best;
                const bool hl = ClipToBox(of, invD, flat, nodes[left].bmin, nodes[left].bmax, la0, la1);
                const bool hr = ClipToBox(of, invD, flat, nodes[right].bmin, nodes[right].bmax, ra0, ra1);
                if (hl && hr) {
                    if (la0 <= ra0) { stack[sp++] = right; stack[sp++] = left; }
                    else { stack[sp++] = left;  stack[sp++] = right; }
                }
                else if (hl) stack[sp++] = left;
                else if (hr) stack[sp++] = right;
            }
        }
        if (found) outT = best;
        return found;
    }

    bool MeshBVH::LineHighest(const Mesh& mesh, const XMFLOAT3& o, const XMFLOAT3& d,
        float tMax, float& outT, SubmeshSkip skip) const
    {
        if (nodes.empty()) return false;
        const auto& pos = mesh.Positions();
        const auto& idx = mesh.Indices();
        const float of[3] = { o.x, o.y, o.z };
        float invD[3]; bool flat[3];
        Prepare(d, invD, flat);

        float best = -FLT_MAX;
        bool found = false;
        uint32_t stack[kStackSize];
        int sp = 0;
        stack[sp++] = 0;
        while (sp > 0) {
            const Node& nd = nodes[stack[--sp]];
            float t0 = found ? best : -FLT_MAX, t1 = tMax;      // 今いちばん高い当たりより下の箱は見ない
            if (!ClipToBox(of, invD, flat, nd.bmin, nd.bmax, t0, t1)) continue;
            if (nd.count > 0) {
                for (uint32_t i = nd.start; i < nd.start + nd.count; ++i) {
                    const uint32_t tri = triIds[i];
                    if (Skipped(tri, skip)) continue;                  // 当たり判定なしのサブメッシュ（5-4b）
                    float t;
                    if (LineTriangle(o, d, pos[idx[tri * 3]], pos[idx[tri * 3 + 1]], pos[idx[tri * 3 + 2]], t)
                        && t <= tMax && (!found || t > best)) {
                        best = t; found = true;
                    }
                }
            }
            else if (sp + 2 <= kStackSize) {
                // 高い（t の大きい）ほうの子を先に見る
                const uint32_t left = static_cast<uint32_t>(&nd - nodes.data()) + 1, right = nd.start;
                float la0 = found ? best : -FLT_MAX, la1 = tMax, ra0 = la0, ra1 = tMax;
                const bool hl = ClipToBox(of, invD, flat, nodes[left].bmin, nodes[left].bmax, la0, la1);
                const bool hr = ClipToBox(of, invD, flat, nodes[right].bmin, nodes[right].bmax, ra0, ra1);
                if (hl && hr) {
                    if (la1 >= ra1) { stack[sp++] = right; stack[sp++] = left; }
                    else { stack[sp++] = left;  stack[sp++] = right; }
                }
                else if (hl) stack[sp++] = left;
                else if (hr) stack[sp++] = right;
            }
        }
        if (found) outT = best;
        return found;
    }

} // namespace uel
