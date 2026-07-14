#include <imgui.h>
#include <fstream>
#include <cctype>
#include <string>
#include "GameObject/Components/ViewModelComponent.h"
#include "GameObject/Actor.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/AnimatorComponent.h"
#include "GameObject/Components/PlayerControllerComponent.h" 
#include "GameObject/Components/CrosshairComponent.h"
#include "Graphics/Graphics.h"
#include "Core/Input.h" 
#include "GameObject/Components/EnemyComponent.h"

static int FindAttachNode(Model* m)
{
	const char* keys[] = { "hand", "wrist", "arm" };
	const std::vector<Model::Node>& nodes = m->GetNodes();
	for (const char* k : keys)
		for (int i = 0; i < static_cast<int>(nodes.size()); ++i)
		{
			std::string low = nodes[i].name;
			for (char& ch : low) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
			if (low.find(k) != std::string::npos) return i;
		}
	return -1;
}

static int FindWeaponIdle(Model* m)
{
	const char* names[] = { "Pistol_IDLE", "idle", "Idle", "static" };
	for (const char* n : names) { int i = m->GetAnimationIndex(n); if (i >= 0) return i; }
	return -1;
}

ViewModelComponent::ViewModelComponent(Camera* camera, PlayerControllerComponent* playerCtrl)
	: camera(camera), playerController(playerCtrl) {
}

void ViewModelComponent::Start()
{
	cachedModelComponent = GetActor()->GetComponent<ModelComponent>().get();
	animator = GetActor()->GetComponent<AnimatorComponent>().get();

	if (playerController) attachModel = playerController->GetModelComponent();

	LoadSettings();

	if (animator && cachedModelComponent && cachedModelComponent->GetModel()) {
		int idx = FindWeaponIdle(cachedModelComponent->GetModel().get());
		if (idx >= 0) animator->Play(idx, 0.1f, true);
	}
}

void ViewModelComponent::SwapWeapon(const char* path)
{
	if (!cachedModelComponent) return;
	cachedModelComponent->SetModel(path);
	currentWeaponState = WeaponState::Idle;
	actionTimer = 0.0f;
	if (animator && cachedModelComponent->GetModel()) {
		int idx = FindWeaponIdle(cachedModelComponent->GetModel().get());
		if (idx >= 0) animator->Play(idx, 0.1f, true);
	}
}

void ViewModelComponent::SaveSettings()
{
	std::ofstream ofs("ViewModelSettings.txt");
	if (ofs) {
		ofs << offsetPos.x << " " << offsetPos.y << " " << offsetPos.z << "\n";
		ofs << offsetRot.x << " " << offsetRot.y << " " << offsetRot.z << "\n";
		ofs << offsetScale.x << " " << offsetScale.y << " " << offsetScale.z << "\n";
		ofs << muzzleOffset.x << " " << muzzleOffset.y << " " << muzzleOffset.z << "\n";
		ofs << showDebugMuzzle << "\n";
		ofs << (attachBoneName[0] ? attachBoneName : "none") << "\n";
	}
}

void ViewModelComponent::LoadSettings()
{
	std::ifstream ifs("ViewModelSettings.txt");
	if (ifs) {
		ifs >> offsetPos.x >> offsetPos.y >> offsetPos.z;
		ifs >> offsetRot.x >> offsetRot.y >> offsetRot.z;
		ifs >> offsetScale.x >> offsetScale.y >> offsetScale.z;
		ifs >> muzzleOffset.x >> muzzleOffset.y >> muzzleOffset.z;
		ifs >> showDebugMuzzle;
		std::string bone;
		if (ifs >> bone && bone != "none") strncpy_s(attachBoneName, bone.c_str(), _TRUNCATE);
	}
}

void ViewModelComponent::Update(float elapsedTime)
{
	if (!camera) return;

	DirectX::XMVECTOR R = DirectX::XMLoadFloat3(&camera->GetRight());
	DirectX::XMVECTOR U = DirectX::XMLoadFloat3(&camera->GetUp());
	DirectX::XMVECTOR F = DirectX::XMLoadFloat3(&camera->GetFront());
	DirectX::XMVECTOR E = DirectX::XMLoadFloat3(&camera->GetEye());

	// ---- Weapon placement: attach to a player body bone (world space) ----
	DirectX::XMMATRIX mScale = DirectX::XMMatrixScaling(offsetScale.x, offsetScale.y, offsetScale.z);
	DirectX::XMMATRIX mOffsetRot = DirectX::XMMatrixRotationRollPitchYaw(offsetRot.x, offsetRot.y, offsetRot.z);
	DirectX::XMMATRIX mOffsetPos = DirectX::XMMatrixTranslation(offsetPos.x, offsetPos.y, offsetPos.z);
	DirectX::XMMATRIX mLocal = mScale * mOffsetRot * mOffsetPos;

	DirectX::XMMATRIX mAttach;
	if (attachModel)
	{
		const std::vector<Model::Node>& pnodes = attachModel->GetPoseNodes();
		if (!pnodes.empty())
		{
			int ni = -1;
			if (attachBoneName[0] != '\0' && attachModel->GetModel())
				ni = attachModel->GetModel()->GetNodeIndex(attachBoneName);
			if (ni < 0 && attachModel->GetModel()) ni = FindAttachNode(attachModel->GetModel().get());
			if (ni < 0 || ni >= static_cast<int>(pnodes.size())) ni = 0;
			mAttach = DirectX::XMLoadFloat4x4(&pnodes[ni].worldTransform);
		}
		else mAttach = DirectX::XMMatrixIdentity();
	}
	else
	{
		DirectX::XMMATRIX mCamWorld;
		mCamWorld.r[0] = DirectX::XMVectorSetW(R, 0.0f); mCamWorld.r[1] = DirectX::XMVectorSetW(U, 0.0f);
		mCamWorld.r[2] = DirectX::XMVectorSetW(F, 0.0f); mCamWorld.r[3] = DirectX::XMVectorSetW(E, 1.0f);
		mAttach = mCamWorld;
	}

	DirectX::XMMATRIX mFinal = mLocal * mAttach;

	DirectX::XMVECTOR scaleVec, rotQuatVec, transVec;
	DirectX::XMMatrixDecompose(&scaleVec, &rotQuatVec, &transVec, mFinal);
	DirectX::XMFLOAT3 wPos, wScale; DirectX::XMFLOAT4 wRot;
	DirectX::XMStoreFloat3(&wPos, transVec);
	DirectX::XMStoreFloat4(&wRot, rotQuatVec);
	DirectX::XMStoreFloat3(&wScale, scaleVec);
	GetActor()->SetPosition(wPos);
	GetActor()->SetRotation(wRot);
	GetActor()->SetScale(wScale);

	// Zero-lag: push the bone-attached world straight into the weapon pose
	// (the animator already submitted its node poses this frame).
	if (cachedModelComponent)
	{
		DirectX::XMFLOAT4X4 wm; DirectX::XMStoreFloat4x4(&wm, mFinal);
		cachedModelComponent->ApplyWorldTransform(wm);
	}

	// ---- Muzzle (camera-based aiming) ----
	DirectX::XMVECTOR muzzlePosVec = E;
	muzzlePosVec = DirectX::XMVectorAdd(muzzlePosVec, DirectX::XMVectorScale(R, muzzleOffset.x));
	muzzlePosVec = DirectX::XMVectorAdd(muzzlePosVec, DirectX::XMVectorScale(U, muzzleOffset.y));
	muzzlePosVec = DirectX::XMVectorAdd(muzzlePosVec, DirectX::XMVectorScale(F, muzzleOffset.z));
	DirectX::XMStoreFloat3(&currentMuzzlePos, muzzlePosVec);

	ShapeRenderer* shapeRenderer = Graphics::Instance().GetShapeRenderer();
	PrimitiveRenderer* primRenderer = Graphics::Instance().GetPrimitiveRenderer();

	if (showDebugMuzzle) {
		shapeRenderer->DrawSphere(currentMuzzlePos, 0.008f, { 0.0f, 1.0f, 0.0f, 1.0f });
		float laserLength = 0.5f;
		DirectX::XMVECTOR Mid = DirectX::XMVectorAdd(muzzlePosVec, DirectX::XMVectorScale(F, laserLength * 0.5f));
		float pitch = asin(-DirectX::XMVectorGetY(F));
		float yaw = atan2(DirectX::XMVectorGetX(F), DirectX::XMVectorGetZ(F));
		DirectX::XMMATRIX Rot = DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2) * DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, 0.0f);
		Rot.r[3] = DirectX::XMVectorSetW(Mid, 1.0f);
		DirectX::XMFLOAT4X4 transform; DirectX::XMStoreFloat4x4(&transform, Rot);
		shapeRenderer->DrawCapsule(transform, 0.002f, laserLength, { 0.0f, 1.0f, 0.0f, 0.8f });
	}

	if (!animator || !cachedModelComponent || !cachedModelComponent->GetModel()) return;
	Model* model = cachedModelComponent->GetModel().get();

	bool isActionPlaying = false;
	if (actionTimer > 0.0f) {
		actionTimer -= elapsedTime;
		isActionPlaying = true;
	}

	WeaponState nextState = currentWeaponState;

	if (!isActionPlaying) {
		Mouse& mouse = Input::Instance().GetMouse();
		if (mouse.GetButtonDown() & Mouse::BTN_LEFT) {
			if (currentAmmo > 0) nextState = WeaponState::Fire;
			else nextState = WeaponState::Reload;
		}
		else if (GetAsyncKeyState('R') & 0x8000) {
			if (currentAmmo < maxAmmo) nextState = WeaponState::Reload;
		}
		else if (playerController) {
			PlayerState pState = playerController->GetCurrentState();
			if (pState == PlayerState::Run) nextState = WeaponState::Run;
			else if (pState == PlayerState::Walk) nextState = WeaponState::Walk;
			else if (pState == PlayerState::Jump || pState == PlayerState::Fall) nextState = WeaponState::Jump;
			else nextState = WeaponState::Idle;
		}
	}

	if (currentWeaponState != nextState) {
		currentWeaponState = nextState;
		int animIdx = -1; float fadeTime = 0.1f; bool loop = true;

		animator->SetAnimSpeed(1.0f);

		switch (currentWeaponState) {
		case WeaponState::Idle: animIdx = model->GetAnimationIndex("Pistol_IDLE"); break;
		case WeaponState::Walk: animIdx = model->GetAnimationIndex("Pistol_WALK"); fadeTime = 0.2f; break;
		case WeaponState::Run:
			animIdx = model->GetAnimationIndex("Pistol_RUN");
			fadeTime = 0.2f;
			if (animIdx < 0) { animIdx = model->GetAnimationIndex("Pistol_WALK"); animator->SetAnimSpeed(1.5f); }
			break;
		case WeaponState::Jump: animIdx = model->GetAnimationIndex("Pistol_JUMP_START"); loop = false; break;
		case WeaponState::Reload:
			currentAmmo = maxAmmo;
			animIdx = model->GetAnimationIndex("Pistol_RELOAD"); loop = false;
			if (animIdx >= 0) actionTimer = model->GetAnimations().at(animIdx).secondsLength;
			break;
		case WeaponState::Fire:
			currentAmmo--;
			animIdx = model->GetAnimationIndex("Pistol_FIRE");
			loop = false; fadeTime = 0.05f;
			if (animIdx >= 0) actionTimer = model->GetAnimations().at(animIdx).secondsLength;

			if (playerController && camera) {
				auto crosshair = GetActor()->GetComponent<CrosshairComponent>();
				if (crosshair) crosshair->AddDynamicOffset(15.0f);

				DirectX::XMFLOAT3 origin = camera->GetEye();
				DirectX::XMFLOAT3 dir = camera->GetFront();

				DirectX::XMFLOAT3 hitPos, hitNorm; float wallDist = 0.0f;
				bool wallHit = playerController->RayCast(origin, dir, 1000.0f, wallDist, hitPos, hitNorm);

				float enemyDist = 0.0f;
				EnemyComponent* enemy = EnemyComponent::Raycast(origin, dir, 1000.0f, enemyDist);

				vfxList.push_back({ currentMuzzlePos, {0,0,0}, {0,0,0}, {1.0f, 0.8f, 0.2f, 1.0f}, 0.05f, 0.05f, 0 });

				if (enemy && (!wallHit || enemyDist < wallDist)) {
					enemy->ApplyDamage(34.0f);
					hitMarkerTimer = 1.0f;
					DirectX::XMVECTOR hp = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&origin), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&dir), enemyDist));
					DirectX::XMFLOAT3 enemyHitPos; DirectX::XMStoreFloat3(&enemyHitPos, hp);
					vfxList.push_back({ currentMuzzlePos, {0,0,0}, enemyHitPos, {1.0f, 0.3f, 0.2f, 0.9f}, 0.15f, 0.15f, 1 });
				}
				else if (wallHit) {
					lastHitPos = hitPos; hitMarkerTimer = 1.0f;
					vfxList.push_back({ currentMuzzlePos, {0,0,0}, hitPos, {1.0f, 0.9f, 0.0f, 0.8f}, 0.15f, 0.15f, 1 });
					DirectX::XMVECTOR decalPos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&hitPos), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&hitNorm), 0.01f));
					DirectX::XMFLOAT3 decalPosF; DirectX::XMStoreFloat3(&decalPosF, decalPos);
					vfxList.push_back({ decalPosF, hitNorm, {0,0,0}, {1.0f, 1.0f, 1.0f, 1.0f}, 5.0f, 5.0f, 2 });
				}
			}
			break;
		}
		if (animIdx >= 0) animator->Play(animIdx, fadeTime, loop);
	}

	for (auto& effect : vfxList) {
		if (effect.lifeTime <= 0) continue;
		effect.lifeTime -= elapsedTime;
		float alpha = effect.lifeTime / effect.maxLifeTime;
		DirectX::XMFLOAT4 color = effect.color;
		color.w *= alpha;

		if (effect.type == 0) {
			shapeRenderer->DrawSphere(effect.position, 0.05f + (1.0f - alpha) * 0.1f, color);
		}
		else if (effect.type == 1) {
			DirectX::XMVECTOR S = DirectX::XMLoadFloat3(&effect.position);
			DirectX::XMVECTOR Ev = DirectX::XMLoadFloat3(&effect.endPosition);
			DirectX::XMVECTOR Dir = DirectX::XMVectorSubtract(Ev, S);
			float length = DirectX::XMVectorGetX(DirectX::XMVector3Length(Dir));
			if (length > 0.001f) {
				Dir = DirectX::XMVector3Normalize(Dir);
				DirectX::XMVECTOR Mid = DirectX::XMVectorAdd(S, DirectX::XMVectorScale(Dir, length * 0.5f));
				float pitch = asin(-DirectX::XMVectorGetY(Dir));
				float yaw = atan2(DirectX::XMVectorGetX(Dir), DirectX::XMVectorGetZ(Dir));
				DirectX::XMMATRIX Rot = DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2) * DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, 0.0f);
				Rot.r[3] = DirectX::XMVectorSetW(Mid, 1.0f);
				DirectX::XMFLOAT4X4 transform; DirectX::XMStoreFloat4x4(&transform, Rot);
				shapeRenderer->DrawCapsule(transform, 0.005f, length, color);
			}
		}
		else if (effect.type == 2) {
			DirectX::XMVECTOR N = DirectX::XMLoadFloat3(&effect.normal);
			DirectX::XMVECTOR UpAxis = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			if (fabsf(effect.normal.y) > 0.99f) UpAxis = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

			DirectX::XMVECTOR Right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(N, UpAxis));
			DirectX::XMVECTOR Top = DirectX::XMVector3Cross(Right, N);

			DirectX::XMVECTOR P = DirectX::XMLoadFloat3(&effect.position);
			float s = 0.05f;

			DirectX::XMVECTOR v0 = DirectX::XMVectorAdd(P, DirectX::XMVectorSubtract(DirectX::XMVectorScale(Top, s), DirectX::XMVectorScale(Right, s)));
			DirectX::XMVECTOR v1 = DirectX::XMVectorAdd(P, DirectX::XMVectorAdd(DirectX::XMVectorScale(Top, s), DirectX::XMVectorScale(Right, s)));
			DirectX::XMVECTOR v2 = DirectX::XMVectorSubtract(P, DirectX::XMVectorSubtract(DirectX::XMVectorScale(Top, s), DirectX::XMVectorScale(Right, s)));
			DirectX::XMVECTOR v3 = DirectX::XMVectorSubtract(P, DirectX::XMVectorAdd(DirectX::XMVectorScale(Top, s), DirectX::XMVectorScale(Right, s)));

			DirectX::XMFLOAT3 p0, p1, p2, p3;
			DirectX::XMStoreFloat3(&p0, v0);
			DirectX::XMStoreFloat3(&p1, v1);
			DirectX::XMStoreFloat3(&p2, v2);
			DirectX::XMStoreFloat3(&p3, v3);

			primRenderer->AddVertex(p0, color, { 0.0f, 0.0f });
			primRenderer->AddVertex(p1, color, { 1.0f, 0.0f });
			primRenderer->AddVertex(p2, color, { 1.0f, 1.0f });

			primRenderer->AddVertex(p0, color, { 0.0f, 0.0f });
			primRenderer->AddVertex(p2, color, { 1.0f, 1.0f });
			primRenderer->AddVertex(p3, color, { 0.0f, 1.0f });
		}
	}
	vfxList.erase(std::remove_if(vfxList.begin(), vfxList.end(), [](const VfxEffect& e) { return e.lifeTime <= 0; }), vfxList.end());
}

void ViewModelComponent::OnGUI()
{
	if (hitMarkerTimer > 0.0f && camera) {
		hitMarkerTimer -= ImGui::GetIO().DeltaTime;
		DirectX::XMVECTOR R = DirectX::XMLoadFloat3(&camera->GetRight());
		DirectX::XMVECTOR U = DirectX::XMLoadFloat3(&camera->GetUp());
		DirectX::XMVECTOR F = DirectX::XMLoadFloat3(&camera->GetFront());
		DirectX::XMVECTOR E = DirectX::XMLoadFloat3(&camera->GetEye());
		DirectX::XMMATRIX mCamWorld;
		mCamWorld.r[0] = DirectX::XMVectorSetW(R, 0.0f); mCamWorld.r[1] = DirectX::XMVectorSetW(U, 0.0f);
		mCamWorld.r[2] = DirectX::XMVectorSetW(F, 0.0f); mCamWorld.r[3] = DirectX::XMVectorSetW(E, 1.0f);
		DirectX::XMMATRIX mView = DirectX::XMMatrixInverse(nullptr, mCamWorld);

		float actualW = Graphics::Instance().GetScreenWidth(); float actualH = Graphics::Instance().GetScreenHeight();
		DirectX::XMMATRIX mProj = DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(60), actualW / actualH, 0.05f, 1000.0f);
		DirectX::XMMATRIX viewProj = mView * mProj;
		DirectX::XMVECTOR vHit = DirectX::XMVectorSetW(DirectX::XMLoadFloat3(&lastHitPos), 1.0f);
		DirectX::XMVECTOR vScreen = DirectX::XMVector4Transform(vHit, viewProj);

		float w = DirectX::XMVectorGetW(vScreen);
		if (w > 0.0f) {
			float nx = DirectX::XMVectorGetX(vScreen) / w; float ny = DirectX::XMVectorGetY(vScreen) / w;
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			float cx = viewport->Pos.x + (nx + 1.0f) * 0.5f * viewport->Size.x;
			float cy = viewport->Pos.y + (1.0f - ny) * 0.5f * viewport->Size.y;

			ImGui::SetNextWindowPos(viewport->Pos); ImGui::SetNextWindowSize(viewport->Size);
			ImGui::SetNextWindowViewport(viewport->ID); ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
			ImGui::Begin("VFXOverlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImU32 color = ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 0.0f, 0.0f, hitMarkerTimer > 1.0f ? 1.0f : hitMarkerTimer));
			drawList->AddLine(ImVec2(cx - 10, cy - 10), ImVec2(cx + 10, cy + 10), color, 2.0f);
			drawList->AddLine(ImVec2(cx + 10, cy - 10), ImVec2(cx - 10, cy + 10), color, 2.0f);
			drawList->AddCircleFilled(ImVec2(cx, cy), 3.0f, color);
			ImGui::End(); ImGui::PopStyleVar(2);
		}
	}

	ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1), u8"▼ 装備する武器モデル");
	const char* weapons[] = { "Pistol", "Rifle", "Staff", "ShootGun" };
	for (const char* wname : weapons)
	{
		char path[128];
		sprintf_s(path, "Data/Model/Weapon/%s.glb", wname);
		if (ImGui::Selectable(wname)) SwapWeapon(path);
	}
	if (cachedModelComponent)
		ImGui::TextDisabled("Current: %s", cachedModelComponent->GetModelPath().c_str());
	ImGui::Separator();

	ImGui::TextColored(ImVec4(1, 0.5f, 1, 1), u8"▼ アタッチ / 位置補正");
	ImGui::InputText(u8"アタッチ先ボーン(空=自動)", attachBoneName, sizeof(attachBoneName));
	if (attachModel && attachModel->GetModel())
	{
		int ni = (attachBoneName[0] != '\0') ? attachModel->GetModel()->GetNodeIndex(attachBoneName) : FindAttachNode(attachModel->GetModel().get());
		if (ni >= 0) ImGui::Text(u8"検出ボーン: [%d] %s", ni, attachModel->GetModel()->GetNodes()[ni].name.c_str());
		else ImGui::TextDisabled(u8"ボーン未検出 -> ルート接続");
	}
	ImGui::DragFloat3(u8"スケール", &offsetScale.x, 0.01f, 0.0001f, 100.0f, "%.4f");
	ImGui::DragFloat3(u8"位置オフセット", &offsetPos.x, 0.01f);
	ImGui::DragFloat3(u8"回転オフセット", &offsetRot.x, 0.01f, -DirectX::XM_PI, DirectX::XM_PI);
	if (ImGui::Button(u8"【 補正設定を保存 】")) SaveSettings();
	ImGui::Separator();

	ImGui::TextColored(ImVec4(1, 1, 0, 1), u8"▼ 弾薬");
	ImGui::Text(u8"%d / %d", currentAmmo, maxAmmo);
	ImGui::Checkbox(u8"銃口デバッグ表示", &showDebugMuzzle);
	ImGui::DragFloat3(u8"銃口オフセット", &muzzleOffset.x, 0.01f);
}