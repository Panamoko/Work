#include "Graphics/Graphics.h"
#include "Graphics/GpuResourceUtils.h"
#include "Scene/SceneGame.h"
#include "GameObject/Actor.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/PlayerControllerComponent.h"
#include "GameObject/Components/AnimatorComponent.h"
#include "GameObject/Components/ViewModelComponent.h"
#include "GameObject/Components/CrosshairComponent.h"
#include "GameObject/Components/EnemyComponent.h"
#include "GameObject/Components/HealthComponent.h"
#include "Core/Input.h"
#include <imgui.h>
#include <ImGuizmo.h>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cfloat>
#include <windows.h>
#pragma comment(lib, "d3dcompiler.lib")

namespace
{
	float Dot3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	float Dist3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
	{
		DirectX::XMFLOAT3 d = { a.x - b.x, a.y - b.y, a.z - b.z };
		return sqrtf(Dot3(d, d));
	}
}

// ---- Selection ----
std::shared_ptr<class Actor> SceneGame::PickActor(const DirectX::XMFLOAT3& rO, const DirectX::XMFLOAT3& rD)
{
	std::shared_ptr<class Actor> best;
	float bestT = FLT_MAX;

	for (const std::shared_ptr<class Actor>& a : ActorManager::Instance().GetActors())
	{
		if (!a) continue;
		const char* n = a->GetName();
		if (!strcmp(n, "Stage") || !strcmp(n, "Player") || !strcmp(n, "ViewModel")) continue;

		float dist;
		if (RayActorAABB(a, rO, rD, dist) && dist < bestT)
		{
			bestT = dist;
			best = a;
		}
	}
	return best;
}

void SceneGame::UpdateGizmo()
{
	Mouse& mouse = Input::Instance().GetMouse();
	bool rmb = (mouse.GetButton() & Mouse::BTN_RIGHT) != 0;

	// W/E/R switch gizmo operation (only when not flying / not typing).
	if (!rmb && !ImGui::GetIO().WantCaptureKeyboard)
	{
		if (GetAsyncKeyState('W') & 0x8000) gizmoMode = 0; // move
		if (GetAsyncKeyState('E') & 0x8000) gizmoMode = 1; // rotate
		if (GetAsyncKeyState('R') & 0x8000) gizmoMode = 2; // scale
	}

	// Left click selects an actor (ignored while an ImGuizmo handle is in use).
	bool lmbDown = (mouse.GetButtonDown() & Mouse::BTN_LEFT) != 0;
	if (lmbDown && !ImGui::GetIO().WantCaptureMouse && !ImGuizmo::IsUsing())
	{
		DirectX::XMFLOAT3 rO, rD;
		if (ScreenRay(rO, rD))
		{
			std::shared_ptr<class Actor> picked = PickActor(rO, rD);
			if (picked) ActorManager::Instance().SetSelected(picked); // keep selection on empty click
		}
	}
}

void SceneGame::DrawImGuizmo()
{
	if (mode != EditorMode::Edit) return;
	std::shared_ptr<class Actor> sel = ActorManager::Instance().GetSelectedActor();
	if (!sel) return;

	using namespace DirectX;

	// Build view / projection like Render().
	XMVECTOR R = XMLoadFloat3(&camera.GetRight());
	XMVECTOR U = XMLoadFloat3(&camera.GetUp());
	XMVECTOR F = XMLoadFloat3(&camera.GetFront());
	XMVECTOR E = XMLoadFloat3(&camera.GetEye());
	XMMATRIX camWorld;
	camWorld.r[0] = XMVectorSetW(R, 0.0f); camWorld.r[1] = XMVectorSetW(U, 0.0f);
	camWorld.r[2] = XMVectorSetW(F, 0.0f); camWorld.r[3] = XMVectorSetW(E, 1.0f);
	XMMATRIX viewM = XMMatrixInverse(nullptr, camWorld);

	float aspect = Graphics::Instance().GetScreenWidth() / Graphics::Instance().GetScreenHeight();
	XMMATRIX projM = XMMatrixPerspectiveFovLH(XMConvertToRadians(60), aspect, 0.05f, 1000.0f);

	XMFLOAT4X4 view, proj, model;
	XMStoreFloat4x4(&view, viewM);
	XMStoreFloat4x4(&proj, projM);
	model = sel->GetTransform();

	ImGuizmo::OPERATION op =
		(gizmoMode == 0) ? ImGuizmo::TRANSLATE :
		(gizmoMode == 1) ? ImGuizmo::ROTATE : ImGuizmo::SCALE;
	ImGuizmo::MODE gmode = (gizmoMode == 2) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

	ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());

	if (ImGuizmo::Manipulate(&view._11, &proj._11, op, gmode, &model._11))
	{
		XMVECTOR s, q, t;
		if (XMMatrixDecompose(&s, &q, &t, XMLoadFloat4x4(&model)))
		{
			XMFLOAT3 pos, scl; XMFLOAT4 rot;
			XMStoreFloat3(&pos, t);
			XMStoreFloat3(&scl, s);
			XMStoreFloat4(&rot, q);
			sel->SetPosition(pos);
			sel->SetRotation(rot);
			sel->SetScale(scl);
		}
	}
}


bool SceneGame::GetActorAABB(const std::shared_ptr<class Actor>& actor, DirectX::XMFLOAT3& outMin, DirectX::XMFLOAT3& outMax)
{
	if (!actor) return false;
	auto mc = actor->GetComponent<ModelComponent>();
	if (!mc) return false;
	Model* model = mc->GetModel().get();
	if (!model) return false;

	auto it = aabbCache.find(model);
	if (it == aabbCache.end())
	{
		DirectX::XMFLOAT3 mn = { FLT_MAX, FLT_MAX, FLT_MAX };
		DirectX::XMFLOAT3 mx = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		for (const Model::Mesh& mesh : model->GetMeshes())
		{
			DirectX::XMMATRIX nodeW = DirectX::XMLoadFloat4x4(&mesh.node->worldTransform);
			for (const Model::Vertex& v : mesh.vertices)
			{
				DirectX::XMVECTOR p = DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&v.position), nodeW);
				DirectX::XMFLOAT3 wp; DirectX::XMStoreFloat3(&wp, p);
				mn.x = (std::min)(mn.x, wp.x); mn.y = (std::min)(mn.y, wp.y); mn.z = (std::min)(mn.z, wp.z);
				mx.x = (std::max)(mx.x, wp.x); mx.y = (std::max)(mx.y, wp.y); mx.z = (std::max)(mx.z, wp.z);
			}
		}
		if (mn.x > mx.x) { mn = { -0.5f,-0.5f,-0.5f }; mx = { 0.5f,0.5f,0.5f }; }
		aabbCache[model] = { mn, mx };
		it = aabbCache.find(model);
	}
	outMin = it->second.first;
	outMax = it->second.second;
	return true;
}

bool SceneGame::RayActorAABB(const std::shared_ptr<class Actor>& actor, const DirectX::XMFLOAT3& rO, const DirectX::XMFLOAT3& rD, float& outDist)
{
	DirectX::XMFLOAT3 mn, mx;
	if (!GetActorAABB(actor, mn, mx)) return false;

	using namespace DirectX;
	XMMATRIX W = XMLoadFloat4x4(&actor->GetTransform());
	XMMATRIX invW = XMMatrixInverse(nullptr, W);
	XMFLOAT3 o, d;
	XMStoreFloat3(&o, XMVector3TransformCoord(XMLoadFloat3(&rO), invW));
	XMStoreFloat3(&d, XMVector3TransformNormal(XMLoadFloat3(&rD), invW));

	float tmin = -FLT_MAX, tmax = FLT_MAX;
	for (int i = 0; i < 3; ++i)
	{
		float O = (&o.x)[i], D = (&d.x)[i], Mn = (&mn.x)[i], Mx = (&mx.x)[i];
		if (fabsf(D) < 1e-8f) { if (O < Mn || O > Mx) return false; }
		else
		{
			float t1 = (Mn - O) / D, t2 = (Mx - O) / D;
			if (t1 > t2) { float t = t1; t1 = t2; t2 = t; }
			if (t1 > tmin) tmin = t1;
			if (t2 < tmax) tmax = t2;
		}
	}
	if (tmax < tmin || tmax < 0.0f) return false;

	float t = (tmin >= 0.0f) ? tmin : tmax;
	XMFLOAT3 localHit = { o.x + d.x * t, o.y + d.y * t, o.z + d.z * t };
	XMFLOAT3 worldHit;
	XMStoreFloat3(&worldHit, XMVector3TransformCoord(XMLoadFloat3(&localHit), W));
	outDist = Dist3(worldHit, rO);
	return true;
}

void SceneGame::DrawSelectionOutline(const DirectX::XMFLOAT4X4& view, const DirectX::XMFLOAT4X4& proj)
{
	if (mode != EditorMode::Edit) return;
	std::shared_ptr<class Actor> sel = ActorManager::Instance().GetSelectedActor();
	if (!sel) return;

	DirectX::XMFLOAT3 mn, mx;
	if (!GetActorAABB(sel, mn, mx)) return;

	using namespace DirectX;
	XMMATRIX W = XMLoadFloat4x4(&sel->GetTransform());
	XMFLOAT3 pts[8] = {
		{ mn.x,mn.y,mn.z }, { mx.x,mn.y,mn.z }, { mx.x,mn.y,mx.z }, { mn.x,mn.y,mx.z },
		{ mn.x,mx.y,mn.z }, { mx.x,mx.y,mn.z }, { mx.x,mx.y,mx.z }, { mn.x,mx.y,mx.z }
	};
	XMFLOAT3 c[8];
	for (int i = 0; i < 8; ++i)
		XMStoreFloat3(&c[i], XMVector3TransformCoord(XMLoadFloat3(&pts[i]), W));

	const int edges[12][2] = {
		{0,1},{1,2},{2,3},{3,0}, {4,5},{5,6},{6,7},{7,4}, {0,4},{1,5},{2,6},{3,7}
	};

	PrimitiveRenderer* prim = Graphics::Instance().GetPrimitiveRenderer();
	const XMFLOAT4 col = { 1.0f, 0.6f, 0.0f, 1.0f };
	for (int e = 0; e < 12; ++e)
	{
		prim->AddVertex(c[edges[e][0]], col);
		prim->AddVertex(c[edges[e][1]], col);
	}

	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();
	RenderState* rs = Graphics::Instance().GetRenderState();
	dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::NoTestNoWrite), 0);
	prim->SetTexture(nullptr);
	prim->Render(dc, view, proj, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	dc->OMSetDepthStencilState(rs->GetDepthStencilState(DepthState::TestAndWrite), 0);
}