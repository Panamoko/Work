#include <algorithm>
#include <DirectXMath.h>
#include "SphereCast.h"

// 外部の点に対する三角形内部の最近点を算出する
bool GetClosestPos_PointTriangle(
	const DirectX::XMVECTOR& Point,
	const DirectX::XMVECTOR TrianglePos[3],
	DirectX::XMVECTOR& nearPos,
	DirectX::XMVECTOR* nearTriPos1,
	DirectX::XMVECTOR* nearTriPos2)
{
	// pointがTrianglePos[0]の外側の頂点領域にあるかチェック
	DirectX::XMVECTOR Vec01 = DirectX::XMVectorSubtract(TrianglePos[1], TrianglePos[0]);
	DirectX::XMVECTOR Vec02 = DirectX::XMVectorSubtract(TrianglePos[2], TrianglePos[0]);
	DirectX::XMVECTOR Vec0P = DirectX::XMVectorSubtract(Point, TrianglePos[0]);
	float d1 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Vec01, Vec0P));
	float d2 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Vec02, Vec0P));

	if (d1 <= 0.0f && d2 <= 0.0f)
	{
		nearPos = TrianglePos[0];
		if (nearTriPos1)
		{
			*nearTriPos1 = TrianglePos[0];
		}
		return false;
	}

	// pointがTrianglePos[1]の外側の頂点領域にあるかチェック
	DirectX::XMVECTOR Vec1P = DirectX::XMVectorSubtract(Point, TrianglePos[1]);
	float d3 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Vec01, Vec1P));
	float d4 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Vec02, Vec1P));

	if (d3 >= 0.0f && d4 <= d3)
	{
		nearPos = TrianglePos[1];
		if (nearTriPos1)
		{
			*nearTriPos1 = TrianglePos[1];
		}
		return false;
	}

	// pointがTrianglePos[0]とTrianglePos[1]の外側の辺領域にあるかチェック
	float scalar3_01 = d1 * d4 - d3 * d2;

	if (scalar3_01 <= 0.0f && d1 >= 0.0f && d3 <= 0.0f)
	{
		float t = d1 / (d1 - d3);
		nearPos = DirectX::XMVectorAdd(TrianglePos[0], DirectX::XMVectorScale(Vec01, t));
		if (nearTriPos1)
		{
			*nearTriPos1 = TrianglePos[0];
		}
		if (nearTriPos2)
		{
			*nearTriPos2 = TrianglePos[1];
		}
		return false;
	}

	// pointがTrianglePos[2]の外側の頂点領域にあるかチェック
	DirectX::XMVECTOR Vec2P = DirectX::XMVectorSubtract(Point, TrianglePos[2]);
	float d5 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Vec01, Vec2P));
	float d6 = DirectX::XMVectorGetX(DirectX::XMVector3Dot(Vec02, Vec2P));

	if (d6 >= 0.0f && d5 <= d6)
	{
		nearPos = TrianglePos[2];
		if (nearTriPos1)
		{
			*nearTriPos1 = TrianglePos[2];
		}
		return false;
	}

	// pointがTrianglePos[0]とTrianglePos[2]の外側の辺領域にあるかチェック
	float scalar3_02 = d5 * d2 - d1 * d6;

	if (scalar3_02 <= 0.0f && d2 >= 0.0f && d6 <= 0.0f)
	{
		float t = d2 / (d2 - d6);
		nearPos = DirectX::XMVectorAdd(TrianglePos[0], DirectX::XMVectorScale(Vec02, t));
		if (nearTriPos1)
		{
			*nearTriPos1 = TrianglePos[0];
		}
		if (nearTriPos2)
		{
			*nearTriPos2 = TrianglePos[2];
		}
		return false;
	}

	// pointがTrianglePos[1]とTrianglePos[2]の外側の辺領域にあるかチェック
	float scalar3_12 = d3 * d6 - d5 * d4;

	if (scalar3_12 <= 0.0f && d4 >= d3 && d5 >= d6)
	{
		float t = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		nearPos = DirectX::XMVectorAdd(TrianglePos[1], DirectX::XMVectorScale(DirectX::XMVectorSubtract(TrianglePos[2], TrianglePos[1]), t));
		if (nearTriPos1)
		{
			*nearTriPos1 = TrianglePos[1];
		}
		if (nearTriPos2)
		{
			*nearTriPos2 = TrianglePos[2];
		}
		return false;
	}

	// ここまでくれば、nearPosは三角形の内部にある
	float denom = 1.0f / (scalar3_01 + scalar3_02 + scalar3_12);
	float t01 = scalar3_02 * denom;
	float t02 = scalar3_01 * denom;
	nearPos = DirectX::XMVectorAdd(TrianglePos[0], DirectX::XMVectorAdd(DirectX::XMVectorScale(Vec01, t01), DirectX::XMVectorScale(Vec02, t02)));
	return true;
}

// レイVs三角形
bool IntersectRayVsTriangle(
	const DirectX::XMVECTOR& rayStart, const DirectX::XMVECTOR& rayDirection, const float rayDist,
	const DirectX::XMVECTOR trianglePos[3],
	SphereCastResult* result)
{
	DirectX::XMVECTOR ab = DirectX::XMVectorSubtract(trianglePos[1], trianglePos[0]);
	DirectX::XMVECTOR ac = DirectX::XMVectorSubtract(trianglePos[2], trianglePos[0]);
	DirectX::XMVECTOR norm = DirectX::XMVector3Cross(ab, ac);
	DirectX::XMVECTOR qp = DirectX::XMVectorSubtract(rayStart, DirectX::XMVectorAdd(rayStart, DirectX::XMVectorScale(rayDirection, rayDist)));
	float d = DirectX::XMVectorGetX(DirectX::XMVector3Dot(norm, qp));

	if (d > 0.0f)	// 表側から交差しているときのみ判定を行う
	{
		if (fabs(d) > 1e-6f)	// 平行確認
		{
			DirectX::XMVECTOR ap = DirectX::XMVectorSubtract(rayStart, trianglePos[0]);
			float t = DirectX::XMVectorGetX(DirectX::XMVector3Dot(norm, ap));
			if (t >= 0.0f && t < d)		// レイの向きと長さ確認
			{
				DirectX::XMVECTOR e = DirectX::XMVector3Cross(qp, ap);
				float v = DirectX::XMVectorGetX(DirectX::XMVector3Dot(ac, e));
				if (v >= 0.0f && v <= d)
				{
					float w = -1 * DirectX::XMVectorGetX(DirectX::XMVector3Dot(ab, e));
					if (w > 0.0f && v + w <= d)
					{
						if (result)
						{
							result->distance = rayDist * t / d;
							result->position = DirectX::XMVectorAdd(rayStart, DirectX::XMVectorScale(rayDirection, result->distance));
							result->normal = DirectX::XMVector3Normalize(norm);
							for (int i = 0; i < 3; i++)
							{
								result->verts[i] = trianglePos[i];
							}
						}

						return true;
					}
				}
			}
		}
	}

	return false;
}

// 球Vs三角形
bool IntersectSphereVsTriangle(
	const DirectX::XMVECTOR& spherePos,
	const float radius,
	const DirectX::XMVECTOR trianglePos[3],
	SphereCastResult* result)
{
	DirectX::XMVECTOR tmpPos = {};
	GetClosestPos_PointTriangle(spherePos, trianglePos, tmpPos);
	DirectX::XMVECTOR vec = DirectX::XMVectorSubtract(spherePos, tmpPos);
	if (result)
	{
		result->position = tmpPos;
		result->normal = DirectX::XMVector3Normalize(vec);
		result->distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(vec));
		for (int i = 0; i < 3; i++)
		{
			result->verts[i] = trianglePos[i];
		}
	}
	return DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(vec)) <= (radius * radius);
}

// レイVs球
bool IntersectRayVsSphere(
	const DirectX::XMVECTOR& start,
	const DirectX::XMVECTOR& end,
	const DirectX::XMVECTOR& spherePos,
	const float radius,
	SphereCastResult* result)
{
	DirectX::XMVECTOR direction = DirectX::XMVectorSubtract(end, start);
	DirectX::XMVECTOR directionNormalize = DirectX::XMVector3Normalize(direction);
	DirectX::XMVECTOR ray2sphere = DirectX::XMVectorSubtract(spherePos, start);
	float projection = DirectX::XMVectorGetX(DirectX::XMVector3Dot(ray2sphere, directionNormalize));
	float distSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(ray2sphere)) - projection * projection;

	if (distSq < radius * radius)
	{
		float distance = projection - sqrtf(radius * radius - distSq);
		if (distance > 0.0f)
		{
			if (distance < DirectX::XMVectorGetX(DirectX::XMVector3Length(direction)))
			{
				if (result)
				{
					result->position = DirectX::XMVectorAdd(start, DirectX::XMVectorScale(directionNormalize, distance));
					result->distance = distance;
					result->normal = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(result->position, spherePos));
				}
				return true;
			}
		}
	}

	return false;
}

// レイVs円柱
bool IntersectRayVsCylinder(
	const DirectX::XMVECTOR& start,
	const DirectX::XMVECTOR& end,
	const DirectX::XMVECTOR& startCylinder,
	const DirectX::XMVECTOR& endCylinder,
	float radius,
	SphereCastResult* result)
{
	DirectX::XMVECTOR d = DirectX::XMVectorSubtract(endCylinder, startCylinder);
	DirectX::XMVECTOR m = DirectX::XMVectorSubtract(start, startCylinder);
	DirectX::XMVECTOR n = DirectX::XMVectorSubtract(end, start);

	float md = DirectX::XMVectorGetX(DirectX::XMVector3Dot(m, d));
	float nd = DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, d));
	float dd = DirectX::XMVectorGetX(DirectX::XMVector3Dot(d, d));

	// 線分が円柱のどちらかの底面に対して完全に外側にあるかどうかを判定
	if (md < 0.0f && md + nd < 0.0f)
	{
		return false;	// 線分が円柱のstartCylinderの外側にある
	}
	if (md > dd && md + nd > dd)
	{
		return false;	// 線分が円柱のendCylinderの外側にある
	}

	float nn = DirectX::XMVectorGetX(DirectX::XMVector3Dot(n, n));
	float a = dd * nn - nd * nd;
	float k = DirectX::XMVectorGetX(DirectX::XMVector3Dot(m, m)) - radius * radius;
	float c = dd * k - md * md;

	// 線分が円柱の軸に対して平行
	if (fabsf(a) < 0.0001f)		// 誤差が出やすい計算なので閾値は大きめ（0.0001f）
	{
		if (c > 0.0f) return false;	// 線分は円柱の外側
		return true;
	}

	// 線分が円柱の軸に対して平行でない
	// 円柱の表面を表す陰関数方程式と直線の方程式の解を求めて交差判定を行う。
	float mn = DirectX::XMVectorGetX(DirectX::XMVector3Dot(m, n));
	float b = dd * mn - nd * md;
	float D = b * b - a * c;	// 判別式

	if (D < 0) return false;	// 実数解がないので交差していない

	float hitDistance = (-b - sqrtf(D)) / a;
	if (hitDistance < 0.0f)
	{
		hitDistance = (-b + sqrtf(D)) / a;
		if (hitDistance < 0.0f)
		{
			return false;	// 交点が線分の外側にあり交差していない
		}
	}
	else if (hitDistance > 1.0f)
	{
		hitDistance = (-b + sqrtf(D)) / a;
		if (hitDistance > 1.0f)
		{
			return false;	// 交点が線分の外側にあり交差していない
		}
	}

	if (md + hitDistance * nd < 0.0f)
	{
		// 円柱のstartCylinder側の底面の外で交差
		float t = -md / nd;

		if (k + t * (2.0f * mn + t * nn) > 0.0f)
		{
			return false;
		}
		hitDistance = t;
	}
	else if (md + hitDistance * nd > dd)
	{
		// 円柱のendCylinder側の底面の外で交差
		float t = (dd - md) / nd;

		if (k + dd - 2.0f * md + t * (2.0f * (mn - nd) + t * nn) > 0.0f)
		{
			return false;
		}
		hitDistance = t;
	}

	// 線分が円柱の底面と底面の間で交差している
	if (hitDistance >= 0.0f && hitDistance <= 1.0f)
	{
		if (result)
		{
			result->position = DirectX::XMVectorAdd(start, DirectX::XMVectorScale(n, hitDistance));
			result->distance = hitDistance * DirectX::XMVectorGetX(DirectX::XMVector3Length(n));
			DirectX::XMVECTOR vec = DirectX::XMVectorSubtract(result->position, startCylinder);
			DirectX::XMVECTOR dNorm = DirectX::XMVector3Normalize(d);
			DirectX::XMVECTOR nearP_OnCenterLine = DirectX::XMVectorMultiplyAdd(dNorm, DirectX::XMVector3Dot(dNorm, vec), startCylinder);
			result->normal = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(result->position, nearP_OnCenterLine));
		}
		return true;
	}

	return false;
}
