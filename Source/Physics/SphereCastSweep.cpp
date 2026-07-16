#include <algorithm>
#include <cfloat>
#include <DirectXMath.h>
#include "SphereCast.h"

// スフィアキャストVs三角形 簡易版
bool IntersectSphereCastVsTriangleEST(
	const DirectX::XMVECTOR& start,
	const DirectX::XMVECTOR& end,
	const float radius,
	const DirectX::XMVECTOR trianglePos[3],
	SphereCastResult* result,
	const float angle)
{
	// 三角形とスタート位置における球が交差している場合は、falseで終了する
	if (IntersectSphereVsTriangle(start, radius, trianglePos)) return false;

	DirectX::XMVECTOR direction = DirectX::XMVectorSubtract(end, start);

	DirectX::XMVECTOR ab = DirectX::XMVectorSubtract(trianglePos[1], trianglePos[0]);
	DirectX::XMVECTOR ac = DirectX::XMVectorSubtract(trianglePos[2], trianglePos[0]);
	DirectX::XMVECTOR norm = DirectX::XMVector3Cross(ab, ac);
	DirectX::XMVECTOR qp = DirectX::XMVectorSubtract(start, end);
	float d = DirectX::XMVectorGetX(DirectX::XMVector3Dot(norm, qp));

	bool angleChk = true;
	if (angle > 0.0f)
	{
		angleChk = d > angle * DirectX::XMVectorGetX(DirectX::XMVector3Length(norm)) * DirectX::XMVectorGetX(DirectX::XMVector3Length(qp));
	}

	if (d > 0.0f && angleChk)	// 表側から交差しているときのみ判定を行う
	{
		if (fabs(d) > 1e-6f)	// 平行確認
		{
			// レイの始点を三角形の法線を元に球表面の最短点に移動させる
			DirectX::XMVECTOR fixVec = DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVectorNegate(norm)), radius);
			DirectX::XMVECTOR startFix = DirectX::XMVectorAdd(start, fixVec);

			DirectX::XMVECTOR ap = DirectX::XMVectorSubtract(startFix, trianglePos[0]);
			float t = DirectX::XMVectorGetX(DirectX::XMVector3Dot(norm, ap));
			if (t < d)	// レイの向きと長さ確認
			{
				// レイと三角形を含む平面の交点の算出
				DirectX::XMVECTOR e = DirectX::XMVector3Cross(qp, ap);
				float v = DirectX::XMVectorGetX(DirectX::XMVector3Dot(ac, e));
				float w = -1 * DirectX::XMVectorGetX(DirectX::XMVector3Dot(ab, e));
				t /= d;
				DirectX::XMVECTOR tmpPosition = DirectX::XMVectorAdd(startFix, DirectX::XMVectorScale(direction, t));

				if (v >= 0.0f && v <= d && w > 0.0f && v + w <= d)
				{
					// レイと三角形を含む平面の交点が三角形の内部
					if (result)
					{
						result->position = tmpPosition;
						result->distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(direction)) * t;
						result->normal = norm;
						result->verts[0] = trianglePos[0];
						result->verts[1] = trianglePos[1];
						result->verts[2] = trianglePos[2];
					}
					return true;
				}
				else
				{
					// レイと三角形を含む平面の交点が三角形の外部

					// 交点に一番近い三角形内部の点を求める
					DirectX::XMVECTOR nearPos = {};
					GetClosestPos_PointTriangle(tmpPosition, trianglePos, nearPos);

					// 交点に一番近い三角形内部の点から、逆向きのレイを発射し、レイの始点にある球体と交差するか確認
					// 交差した場合、交点に一番近い三角形内部の点がスフィアキャストの三角形との交点になる
					if (IntersectRayVsSphere(nearPos, DirectX::XMVectorAdd(nearPos, DirectX::XMVectorNegate(direction)), start, radius))
					{
						if (result)
						{
							result->position = nearPos;
							result->normal = norm;
							result->verts[0] = trianglePos[0];
							result->verts[1] = trianglePos[1];
							result->verts[2] = trianglePos[2];
						}
						return true;
					}
				}
			}
		}
	}

	return false;
}

// スフィアキャストVs三角形 完全版
bool IntersectSphereCastVsTriangle(
	const DirectX::XMVECTOR& start,
	const DirectX::XMVECTOR& end,
	const float radius,
	const DirectX::XMVECTOR trianglePos[3],
	SphereCastResult* result,
	const float angle)
{
	// 三角形とスタート位置における球が交差している場合は、falseで終了する
	if (IntersectSphereVsTriangle(start, radius, trianglePos)) return false;

	DirectX::XMVECTOR ab = DirectX::XMVectorSubtract(trianglePos[1], trianglePos[0]);
	DirectX::XMVECTOR ac = DirectX::XMVectorSubtract(trianglePos[2], trianglePos[0]);
	DirectX::XMVECTOR norm = DirectX::XMVector3Cross(ab, ac);
	DirectX::XMVECTOR qp = DirectX::XMVectorSubtract(start, end);
	float d = DirectX::XMVectorGetX(DirectX::XMVector3Dot(norm, qp));
	bool hitFlg = false;

	bool angleChk = true;
	if (angle > 0.0f)
	{
		angleChk = d > angle * DirectX::XMVectorGetX(DirectX::XMVector3Length(norm)) * DirectX::XMVectorGetX(DirectX::XMVector3Length(qp));
	}

	if (d >= 0.0f && angleChk)	// 表側から交差しているときのみ判定を行う
	{
		// 三角形の各頂点を法線を元に球半径だけ移動させる
		DirectX::XMVECTOR fixVec = DirectX::XMVectorScale(DirectX::XMVector3Normalize(norm), radius);

		// 移動後の三角形と交差するなら、元の三角形の内部でスフィアキャストが交差することが確定
		DirectX::XMVECTOR ap = DirectX::XMVectorSubtract(start, DirectX::XMVectorAdd(trianglePos[0], fixVec));
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
						DirectX::XMVECTOR crossPos = DirectX::XMVectorAdd(start, DirectX::XMVectorScale(qp, -t / d));
						result->distance = DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(crossPos, start)));
						result->position = DirectX::XMVectorSubtract(crossPos, fixVec);
						result->normal = norm;
						result->verts[0] = trianglePos[0];
						result->verts[1] = trianglePos[1];
						result->verts[2] = trianglePos[2];
					}
					return true;
				}
			}
		}

		// 面領域で交差がなければ、ボロノイの各頂点領域、辺領域で交差判定を行い、最短距離を算出する
		enum class IntersectPattern
		{
			enNone = -1,
			enVertex0 = 1,
			enVertex1,
			enVertex2,
			enEdge01,
			enEdge02,
			enEdge12
		};
		SphereCastResult tmpResult = {};
		float distance = 0.0f;
		DirectX::XMVECTOR directionNormalize = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(end, start));
		float minDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(qp));
		IntersectPattern minDistCalcPattern = IntersectPattern::enNone;

		// trianglePos[0] 頂点領域のチェック
		DirectX::XMVECTOR ray2sphere = DirectX::XMVectorSubtract(trianglePos[0], start);
		float projection = DirectX::XMVectorGetX(DirectX::XMVector3Dot(ray2sphere, directionNormalize));
		float distSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(ray2sphere)) - projection * projection;
		if (projection > 0.0f)
		{
			if (distSq < radius * radius)
			{
				distance = projection - sqrtf(radius * radius - distSq);
				if (minDistance > distance)
				{
					minDistance = distance;
					minDistCalcPattern = IntersectPattern::enVertex0;
					hitFlg = true;
				}
			}
		}

		// trianglePos[1] 頂点領域のチェック
		ray2sphere = DirectX::XMVectorSubtract(trianglePos[1], start);
		projection = DirectX::XMVectorGetX(DirectX::XMVector3Dot(ray2sphere, directionNormalize));
		distSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(ray2sphere)) - projection * projection;
		if (projection > 0.0f)
		{
			if (distSq < radius * radius)
			{
				distance = projection - sqrtf(radius * radius - distSq);
				if (minDistance > distance)
				{
					minDistance = distance;
					minDistCalcPattern = IntersectPattern::enVertex1;
					hitFlg = true;
				}
			}
		}

		// trianglePos[2] 頂点領域のチェック
		ray2sphere = DirectX::XMVectorSubtract(trianglePos[2], start);
		projection = DirectX::XMVectorGetX(DirectX::XMVector3Dot(ray2sphere, directionNormalize));
		if (projection > 0.0f)
		{
			distSq = DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(ray2sphere)) - projection * projection;
			if (distSq < radius * radius)
			{
				distance = projection - sqrtf(radius * radius - distSq);
				if (minDistance > distance)
				{
					minDistance = distance;
					minDistCalcPattern = IntersectPattern::enVertex2;
					hitFlg = true;
				}
			}
		}

		DirectX::XMVECTOR tmpPosition = {};
		// trianglePos[0]-trianglePos[1] 辺領域のチェック
		if (IntersectRayVsCylinder(start, end, trianglePos[0], trianglePos[1], radius, &tmpResult))
		{
			if (minDistance > tmpResult.distance)
			{
				minDistance = tmpResult.distance;
				minDistCalcPattern = IntersectPattern::enEdge01;
				tmpPosition = tmpResult.position;
				hitFlg = true;
			}
		}

		// trianglePos[0]-trianglePos[2] 辺領域のチェック
		if (IntersectRayVsCylinder(start, end, trianglePos[0], trianglePos[2], radius, &tmpResult))
		{
			if (minDistance > tmpResult.distance)
			{
				minDistance = tmpResult.distance;
				minDistCalcPattern = IntersectPattern::enEdge02;
				tmpPosition = tmpResult.position;
				hitFlg = true;
			}
		}

		// trianglePos[1]-trianglePos[2] 辺領域のチェック
		if (IntersectRayVsCylinder(start, end, trianglePos[1], trianglePos[2], radius, &tmpResult))
		{
			if (minDistance > tmpResult.distance)
			{
				minDistance = tmpResult.distance;
				minDistCalcPattern = IntersectPattern::enEdge12;
				tmpPosition = tmpResult.position;
				hitFlg = true;
			}
		}

		// 交差が確定し、resultが有効ならHitResult情報を算出する
		if (hitFlg && result)
		{
			result->distance = minDistance;
			result->normal = norm;
			result->verts[0] = trianglePos[0];
			result->verts[1] = trianglePos[1];
			result->verts[2] = trianglePos[2];

			// 交点の算出は当たり方によって分岐
			switch (minDistCalcPattern)
			{
			case IntersectPattern::enVertex0:
				result->position = trianglePos[0];
				break;
			case IntersectPattern::enVertex1:
				result->position = trianglePos[1];
				break;
			case IntersectPattern::enVertex2:
				result->position = trianglePos[2];
				break;
			case IntersectPattern::enEdge01:
			{
				DirectX::XMVECTOR vec0p = DirectX::XMVectorSubtract(tmpPosition, trianglePos[0]);
				DirectX::XMVECTOR vec01Norm = DirectX::XMVector3Normalize(ab);
				result->position = DirectX::XMVectorMultiplyAdd(DirectX::XMVector3Dot(vec0p, vec01Norm), vec01Norm, trianglePos[0]);
			}
			break;
			case IntersectPattern::enEdge02:
			{
				DirectX::XMVECTOR vec0p = DirectX::XMVectorSubtract(tmpPosition, trianglePos[0]);
				DirectX::XMVECTOR vec02Norm = DirectX::XMVector3Normalize(ac);
				result->position = DirectX::XMVectorMultiplyAdd(DirectX::XMVector3Dot(vec0p, vec02Norm), vec02Norm, trianglePos[0]);
			}
			break;
			case IntersectPattern::enEdge12:
			{
				DirectX::XMVECTOR vec1p = DirectX::XMVectorSubtract(tmpPosition, trianglePos[1]);
				DirectX::XMVECTOR vec12Norm = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(trianglePos[2], trianglePos[1]));
				result->position = DirectX::XMVectorMultiplyAdd(DirectX::XMVector3Dot(vec1p, vec12Norm), vec12Norm, trianglePos[1]);
			}
			break;
			default:
				break;
			}
		}
	}

	return hitFlg;
}

// スフィアキャストVs球
bool IntersectSphereCastVsSphere(
	const DirectX::XMVECTOR& start,
	const DirectX::XMVECTOR& end,
	const float sphereCastRadius,
	const DirectX::XMVECTOR& spherePos,
	const float sphereRadius,
	SphereCastResult* result)
{
	SphereCastResult tmpResult = {};
	if (IntersectRayVsSphere(start, end, spherePos, sphereCastRadius + sphereRadius, &tmpResult) && tmpResult.distance > 0.0f)
	{
		if (result)
		{
			DirectX::XMVECTOR vec = DirectX::XMVectorSubtract(spherePos, tmpResult.position);
			result->position = DirectX::XMVectorAdd(tmpResult.position, DirectX::XMVectorScale(DirectX::XMVector3Normalize(vec), sphereCastRadius));
			result->distance = tmpResult.distance;
			result->normal = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(result->position, spherePos));
		}
		return true;
	}

	return false;
}

// スフィアキャストVsカプセル
bool IntersectSphereCastVsCapsule(
	const DirectX::XMVECTOR& start,
	const DirectX::XMVECTOR& end,
	const float sphereCastRadius,
	const DirectX::XMVECTOR& startCapsule,
	const DirectX::XMVECTOR& endCapsule,
	const float capsuleRadius,
	SphereCastResult* result)
{
	DirectX::XMVECTOR nearPos = {};
	float nearDist = FLT_MAX;
	DirectX::XMVECTOR nearNorm = {};
	bool ret = false;
	SphereCastResult tmpResult = {};

	if (IntersectRayVsSphere(start, end, startCapsule, sphereCastRadius + capsuleRadius, &tmpResult) && tmpResult.distance > 0.0f)
	{
		DirectX::XMVECTOR vec = DirectX::XMVectorSubtract(startCapsule, tmpResult.position);
		nearPos = DirectX::XMVectorAdd(tmpResult.position, DirectX::XMVectorScale(DirectX::XMVector3Normalize(vec), sphereCastRadius));
		nearDist = tmpResult.distance;
		nearNorm = tmpResult.normal;
		ret = true;
	}

	if (IntersectRayVsSphere(start, end, endCapsule, sphereCastRadius + capsuleRadius, &tmpResult) && tmpResult.distance > 0.0f)
	{
		if (nearDist > tmpResult.distance)
		{
			DirectX::XMVECTOR vec = DirectX::XMVectorSubtract(endCapsule, tmpResult.position);
			nearPos = DirectX::XMVectorAdd(tmpResult.position, DirectX::XMVectorScale(DirectX::XMVector3Normalize(vec), sphereCastRadius));
			nearDist = tmpResult.distance;
			nearNorm = tmpResult.normal;
			ret = true;
		}
	}

	if (IntersectRayVsCylinder(start, end, startCapsule, endCapsule, sphereCastRadius + capsuleRadius, &tmpResult) && tmpResult.distance > 0.0f)
	{
		if (nearDist >= tmpResult.distance)
		{
			DirectX::XMVECTOR vec = DirectX::XMVectorSubtract(tmpResult.position, startCapsule);
			DirectX::XMVECTOR capsuleVec = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(endCapsule, startCapsule));
			DirectX::XMVECTOR nearCenter = DirectX::XMVectorMultiplyAdd(DirectX::XMVector3Dot(vec, capsuleVec), capsuleVec, startCapsule);
			nearPos = DirectX::XMVectorAdd(DirectX::XMVectorScale(DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(nearCenter, tmpResult.position)), sphereCastRadius), tmpResult.position);
			nearDist = tmpResult.distance;
			nearNorm = tmpResult.normal;
			ret = true;
		}
	}

	if (result)
	{
		result->position = nearPos;
		result->distance = nearDist;
		result->normal = nearNorm;
	}

	return ret;
}

// スフィアキャストVsAABB用の頂点算出関数
inline DirectX::XMVECTOR GetCorner(const DirectX::XMVECTOR& aabbPos, const DirectX::XMVECTOR& aabbRadii, const int v)
{
	DirectX::XMFLOAT3 p = {};
	DirectX::XMStoreFloat3(&p, aabbPos);
	p.x += (v & (1 << 0)) ? DirectX::XMVectorGetX(aabbRadii) : -DirectX::XMVectorGetX(aabbRadii);
	p.y += (v & (1 << 1)) ? DirectX::XMVectorGetY(aabbRadii) : -DirectX::XMVectorGetY(aabbRadii);
	p.z += (v & (1 << 2)) ? DirectX::XMVectorGetZ(aabbRadii) : -DirectX::XMVectorGetZ(aabbRadii);

	return DirectX::XMLoadFloat3(&p);
}

// スフィアキャストVsAABB
bool IntersectSphereCastVsAABB(
	const DirectX::XMVECTOR& start,
	const DirectX::XMVECTOR& end,
	const float sphereCastRadius,
	const DirectX::XMVECTOR& aabbPos,
	const DirectX::XMVECTOR& aabbRadii,
	SphereCastResult* result)
{
	// スフィアキャストの始端、AABBの中心、AABBとカプセル半径のボロノイ領域の半径を
	// ループ処理するため、xyz成分を配列に代入する
	float startPosArray[3] = { DirectX::XMVectorGetX(start), DirectX::XMVectorGetY(start), DirectX::XMVectorGetZ(start) };
	float aabbPosArray[3] = { DirectX::XMVectorGetX(aabbPos), DirectX::XMVectorGetY(aabbPos), DirectX::XMVectorGetZ(aabbPos) };
	float aabbVoronoiRadArray[3] = { DirectX::XMVectorGetX(aabbRadii) + sphereCastRadius, DirectX::XMVectorGetY(aabbRadii) + sphereCastRadius, DirectX::XMVectorGetZ(aabbRadii) + sphereCastRadius };

	// スフィアキャストのベクトルを作り、距離を変数にバックアップしたうえで正規化し、
	// こちらもループ処理するため、xyz成分を配列に代入
	DirectX::XMVECTOR dVec = DirectX::XMVectorSubtract(end, start);
	float rayLength = DirectX::XMVectorGetX(DirectX::XMVector3Length(dVec));
	dVec = DirectX::XMVector3Normalize(dVec);
	float dArray[3] = { DirectX::XMVectorGetX(dVec), DirectX::XMVectorGetY(dVec), DirectX::XMVectorGetZ(dVec) };

	// 直線とスラブの２交点までの距離をtminとtmaxと定義
	float tmin = 0.0f;
	float tmax = FLT_MAX;

	// スラブとの距離を算出し交差しているかの確認と最近点の算出を行う
	for (int i = 0; i < 3; i++)
	{
		//xyz軸との平行確認
		if (fabsf(dArray[i]) < FLT_EPSILON)
		{
			// 平行の場合、位置関係の比較を行い、範囲内になければ交差なしでreturn false
			if (startPosArray[i] < aabbPosArray[i] - aabbVoronoiRadArray[i] || startPosArray[i] > aabbPosArray[i] + aabbVoronoiRadArray[i])
			{
				return false;
			}
		}
		else
		{
			// t1が近スラブ、t2が遠スラブとの距離
			float ood = 1.0f / dArray[i];
			float t1 = (aabbPosArray[i] - aabbVoronoiRadArray[i] - startPosArray[i]) * ood;
			float t2 = (aabbPosArray[i] + aabbVoronoiRadArray[i] - startPosArray[i]) * ood;

			// 遠近が逆転している場合があるので、その場合入れ替えておく
			if (t1 > t2)
			{
				float tmp = t1;
				t1 = t2;
				t2 = tmp;
			}

			// t1がtminよりも大きい場合、tminをt1で更新する
			if (t1 > tmin) tmin = t1;

			// t2がtmaxよりも小さい場合、tmaxをt2で更新する
			if (t2 < tmax) tmax = t2;

			// tminとtmaxの大小関係が逆転するのは、交差していない場合のみなので、その場合はreturn false
			if (tmin > tmax) return false;
		}
	}

	// スラブとの交点がレイの外側にある
	if (rayLength < tmin) return false;

	// ここまで来たらスフィアキャストと拡大したAABB（ボロノイ領域）との交差が確定
	// 交点を割り出す。
	DirectX::XMFLOAT3 point = {
		startPosArray[0] + dArray[0] * tmin,
		startPosArray[1] + dArray[1] * tmin,
		startPosArray[2] + dArray[2] * tmin,
	};

	// 求めた交点から、頂点・面・辺のボロノイ領域のチェックを行う
	// u をマイナス、v をプラスとし、ビットでxyzの向きを管理している
	int u = 0, v = 0;
	if (point.x <= aabbPosArray[0] - DirectX::XMVectorGetX(aabbRadii)) u |= (1 << 0);
	else if (point.x >= aabbPosArray[0] + DirectX::XMVectorGetX(aabbRadii)) v |= (1 << 0);
	if (point.y <= aabbPosArray[1] - DirectX::XMVectorGetY(aabbRadii)) u |= (1 << 1);
	else if (point.y >= aabbPosArray[1] + DirectX::XMVectorGetY(aabbRadii)) v |= (1 << 1);
	if (point.z <= aabbPosArray[2] - DirectX::XMVectorGetZ(aabbRadii)) u |= (1 << 2);
	else if (point.z >= aabbPosArray[2] + DirectX::XMVectorGetZ(aabbRadii)) v |= (1 << 2);

	int mask = u | v;

	if ((mask & (mask - 1)) == 0)	// ボロノイ面領域の場合
	{
		DirectX::XMVECTOR normal = {
			(float)(((u & 1) - (v & 1)) >> 0),
			(float)(((u & 2) - (v & 2)) >> 1),
			(float)(((u & 4) - (v & 4)) >> 2) };
		if (result)
		{
			result->position = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&point), DirectX::XMVectorScale(normal, sphereCastRadius));
			result->normal = normal;
			result->distance = tmin;
		}

		return true;
	}
	else	// ボロノイ頂点＆辺領域の場合
	{
		DirectX::XMVECTOR nearPos = {};
		float nearDist = FLT_MAX;
		DirectX::XMVECTOR nearNorm = {};
		bool ret = false;
		DirectX::XMVECTOR startVertex = GetCorner(aabbPos, aabbRadii, v);
		DirectX::XMVECTOR endVertex = GetCorner(aabbPos, aabbRadii, u ^ 7);

		// ボロノイ頂点領域での交差の場合endVertexが生成されない（startVertexと同じものが出来てしまう）が、
		// 頂点の球に交差せずに隣接する辺領域に交差する場合が存在するためスフィアキャストの向きから最適なendVertexを生成
		if (mask == 7)
		{
			float maxValue = std::max(std::max(fabsf(dArray[0]), fabsf(dArray[1])), fabsf(dArray[2]));
			for (int i = 0; i < 3; i++)
			{
				if (fabsf(dArray[i]) == maxValue)
				{
					int p = u ^ (1 << i);
					endVertex = GetCorner(aabbPos, aabbRadii, p ^ 7);
					break;
				}
			}
		}

		// 以降はスフィアキャストVsカプセルと同じ処理だが、衝突点の算出方法が違うので関数は呼んでいない

		SphereCastResult tmpResult = {};

		// 頂点に球を配置してレイと交差判定
		if (IntersectRayVsSphere(start, end, startVertex, sphereCastRadius, &tmpResult) && tmpResult.distance > 0.0f)
		{
			nearPos = startVertex;
			nearDist = tmpResult.distance;
			nearNorm = tmpResult.normal;
			ret = true;
		}
		if (IntersectRayVsSphere(start, end, endVertex, sphereCastRadius, &tmpResult) && tmpResult.distance > 0.0f)
		{
			if (nearDist > tmpResult.distance)
			{
				nearPos = endVertex;
				nearDist = tmpResult.distance;
				nearNorm = tmpResult.normal;
				ret = true;
			}
		}
		// 辺に円柱を配置してレイと交差判定
		if (IntersectRayVsCylinder(start, end, startVertex, endVertex, sphereCastRadius, &tmpResult) && tmpResult.distance > 0.0f)
		{
			if (nearDist > tmpResult.distance)
			{
				DirectX::XMVECTOR vec = DirectX::XMVectorSubtract(tmpResult.position, startVertex);
				DirectX::XMVECTOR capsuleVec = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(endVertex, startVertex));
				nearPos = DirectX::XMVectorMultiplyAdd(DirectX::XMVector3Dot(vec, capsuleVec), capsuleVec, startVertex);
				nearDist = tmpResult.distance;
				nearNorm = tmpResult.normal;
				ret = true;
			}
		}

		if (ret)
		{
			if (result)
			{
				result->position = nearPos;
				result->distance = nearDist;
				result->normal = nearNorm;
			}
			return true;
		}
	}

	return false;
}