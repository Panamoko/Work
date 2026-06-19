#include <imgui.h>
#include <fstream>
#include "GameObject/Components/ViewModelComponent.h"
#include "GameObject/Actor.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/AnimatorComponent.h"
#include "GameObject/Components/PlayerControllerComponent.h" 
#include "GameObject/Components/CrosshairComponent.h"
#include "Graphics/Graphics.h"
#include "Core/Input.h" 

ViewModelComponent::ViewModelComponent(Camera* camera, PlayerControllerComponent* playerCtrl)
	: camera(camera), playerController(playerCtrl) {
}

void ViewModelComponent::Start()
{
	cachedModelComponent = GetActor()->GetComponent<ModelComponent>().get();
	animator = GetActor()->GetComponent<AnimatorComponent>().get();

	LoadSettings();

	if (animator && cachedModelComponent && cachedModelComponent->GetModel()) {
		int idx = cachedModelComponent->GetModel()->GetAnimationIndex("Pistol_IDLE");
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

		// =======================================================
		// ★追加：銃口オフセットと、デバッグ表示のON/OFF状態も保存する
		// =======================================================
		ofs << muzzleOffset.x << " " << muzzleOffset.y << " " << muzzleOffset.z << "\n";
		ofs << showDebugMuzzle << "\n";
	}
}

void ViewModelComponent::LoadSettings()
{
	std::ifstream ifs("ViewModelSettings.txt");
	if (ifs) {
		ifs >> offsetPos.x >> offsetPos.y >> offsetPos.z;
		ifs >> offsetRot.x >> offsetRot.y >> offsetRot.z;
		ifs >> offsetScale.x >> offsetScale.y >> offsetScale.z;

		// =======================================================
		// ★追加：保存した順番と「全く同じ順番」で読み込む
		// =======================================================
		ifs >> muzzleOffset.x >> muzzleOffset.y >> muzzleOffset.z;
		ifs >> showDebugMuzzle;
	}
}

void ViewModelComponent::Update(float elapsedTime)
{
	if (!camera) return;

	DirectX::XMMATRIX mScale = DirectX::XMMatrixScaling(offsetScale.x, offsetScale.y, offsetScale.z);
	DirectX::XMMATRIX mOffsetRot = DirectX::XMMatrixRotationRollPitchYaw(offsetRot.x, offsetRot.y, offsetRot.z);
	DirectX::XMMATRIX mOffsetPos = DirectX::XMMatrixTranslation(offsetPos.x, offsetPos.y, offsetPos.z);

	DirectX::XMVECTOR R = DirectX::XMLoadFloat3(&camera->GetRight());
	DirectX::XMVECTOR U = DirectX::XMLoadFloat3(&camera->GetUp());
	DirectX::XMVECTOR F = DirectX::XMLoadFloat3(&camera->GetFront());
	DirectX::XMVECTOR E = DirectX::XMLoadFloat3(&camera->GetEye());

	DirectX::XMMATRIX mCamWorld;
	mCamWorld.r[0] = DirectX::XMVectorSetW(R, 0.0f); mCamWorld.r[1] = DirectX::XMVectorSetW(U, 0.0f);
	mCamWorld.r[2] = DirectX::XMVectorSetW(F, 0.0f); mCamWorld.r[3] = DirectX::XMVectorSetW(E, 1.0f);

	DirectX::XMMATRIX mFinal = mScale * mOffsetRot * mOffsetPos * mCamWorld;

	DirectX::XMVECTOR scaleVec, rotQuatVec, transVec;
	DirectX::XMMatrixDecompose(&scaleVec, &rotQuatVec, &transVec, mFinal);

	DirectX::XMFLOAT3 wPos, wScale; DirectX::XMFLOAT4 wRot;
	DirectX::XMStoreFloat3(&wPos, transVec);
	DirectX::XMStoreFloat4(&wRot, rotQuatVec);
	DirectX::XMStoreFloat3(&wScale, scaleVec);

	GetActor()->SetPosition(wPos);
	GetActor()->SetRotation(wRot);
	GetActor()->SetScale(wScale);

	// =======================================================
	// ★プロのハック：めり込んでも位置が完璧に分かる「レーザー」を描画する
	// =======================================================
	DirectX::XMVECTOR muzzlePosVec = E;
	muzzlePosVec = DirectX::XMVectorAdd(muzzlePosVec, DirectX::XMVectorScale(R, muzzleOffset.x));
	muzzlePosVec = DirectX::XMVectorAdd(muzzlePosVec, DirectX::XMVectorScale(U, muzzleOffset.y));
	muzzlePosVec = DirectX::XMVectorAdd(muzzlePosVec, DirectX::XMVectorScale(F, muzzleOffset.z));
	DirectX::XMStoreFloat3(&currentMuzzlePos, muzzlePosVec);

	ShapeRenderer* shapeRenderer = Graphics::Instance().GetShapeRenderer();
	if (showDebugMuzzle) {
		// 1. 基準の球（小さめに設定。銃身にめり込んで見えなくてもOK）
		shapeRenderer->DrawSphere(currentMuzzlePos, 0.008f, { 0.0f, 1.0f, 0.0f, 1.0f });

		// 2. めり込み対策：マズルから前方へ50cm伸びる「緑のレーザー」を追加
		float laserLength = 0.5f;
		DirectX::XMVECTOR Mid = DirectX::XMVectorAdd(muzzlePosVec, DirectX::XMVectorScale(F, laserLength * 0.5f));

		float pitch = asin(-DirectX::XMVectorGetY(F));
		float yaw = atan2(DirectX::XMVectorGetX(F), DirectX::XMVectorGetZ(F));
		DirectX::XMMATRIX Rot = DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2) * DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, 0.0f);
		Rot.r[3] = DirectX::XMVectorSetW(Mid, 1.0f);
		DirectX::XMFLOAT4X4 transform; DirectX::XMStoreFloat4x4(&transform, Rot);

		// 非常に細いカプセルをレーザーとして描画（銃身を突き抜けて表示される）
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
			if (pState == PlayerState::Walk) nextState = WeaponState::Walk;
			else if (pState == PlayerState::Jump || pState == PlayerState::Fall) nextState = WeaponState::Jump;
			else nextState = WeaponState::Idle;
		}
	}

	if (currentWeaponState != nextState) {
		currentWeaponState = nextState;
		int animIdx = -1; float fadeTime = 0.1f; bool loop = true;

		switch (currentWeaponState) {
		case WeaponState::Idle: animIdx = model->GetAnimationIndex("Pistol_IDLE"); break;
		case WeaponState::Walk: animIdx = model->GetAnimationIndex("Pistol_WALK"); fadeTime = 0.2f; break;
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

				DirectX::XMFLOAT3 hitPos, hitNorm; float dist = 0.0f;
				if (playerController->RayCast(camera->GetEye(), camera->GetFront(), 1000.0f, dist, hitPos, hitNorm)) {
					lastHitPos = hitPos; hitMarkerTimer = 1.0f;

					vfxList.push_back({ currentMuzzlePos, {0,0,0}, {0,0,0}, {1.0f, 0.8f, 0.2f, 1.0f}, 0.05f, 0.05f, 0 });
					vfxList.push_back({ currentMuzzlePos, {0,0,0}, hitPos, {1.0f, 0.9f, 0.0f, 0.8f}, 0.15f, 0.15f, 1 });
					DirectX::XMVECTOR decalPos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&hitPos), DirectX::XMVectorScale(DirectX::XMLoadFloat3(&hitNorm), 0.01f));
					DirectX::XMFLOAT3 decalPosF; DirectX::XMStoreFloat3(&decalPosF, decalPos);
					vfxList.push_back({ decalPosF, hitNorm, {0,0,0}, {0.1f, 0.1f, 0.1f, 1.0f}, 5.0f, 5.0f, 2 });
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
		DirectX::XMFLOAT4 color = effect.color; color.w *= alpha;

		if (effect.type == 0) {
			shapeRenderer->DrawSphere(effect.position, 0.05f + (1.0f - alpha) * 0.1f, color);
		}
		else if (effect.type == 1) {
			DirectX::XMVECTOR S = DirectX::XMLoadFloat3(&effect.position);
			DirectX::XMVECTOR E = DirectX::XMLoadFloat3(&effect.endPosition);
			DirectX::XMVECTOR Dir = DirectX::XMVectorSubtract(E, S);
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
			float pitch = asin(-effect.normal.y);
			float yaw = atan2(effect.normal.x, effect.normal.z);
			shapeRenderer->DrawBox(effect.position, { pitch, yaw, 0.0f }, { 0.05f, 0.05f, 0.005f }, color);
		}
	}
	vfxList.erase(std::remove_if(vfxList.begin(), vfxList.end(), [](const VfxEffect& e) { return e.lifeTime <= 0; }), vfxList.end());
}

void ViewModelComponent::OnGUI()
{
	// 2Dのヒットマーカー
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

	// =======================================================
	// インスペクタ（操作パネル）
	// =======================================================
	ImGui::TextColored(ImVec4(1, 0.5f, 1, 1), u8"▼ FPS腕モデル補正設定 (ViewModel)");
	if (ImGui::Button(u8"【 補正設定を保存する 】")) SaveSettings();
	ImGui::Separator();

	ImGui::TextColored(ImVec4(1, 1, 0, 1), u8"▼ 武器ステータス (Ammo)");
	ImGui::Text(u8"弾薬: %d / %d", currentAmmo, maxAmmo);
	ImGui::Separator();

	ImGui::TextColored(ImVec4(0, 1, 0, 1), u8"▼ 銃口(マズル) 位置設定");
	ImGui::Checkbox(u8"銃口の目印(緑の球＋レーザー)を表示する", &showDebugMuzzle);
	ImGui::DragFloat3(u8"銃口オフセット(X,Y,Z)", &muzzleOffset.x, 0.01f);
	ImGui::Text(u8"※カメラから何メートル離れているか");
	ImGui::Separator();

	ImGui::DragFloat3(u8"強制スケール", &offsetScale.x, 0.001f, 0.0001f, 10.0f, "%.4f");
	ImGui::DragFloat3(u8"強制位置オフセット", &offsetPos.x, 0.01f);
	ImGui::DragFloat3(u8"強制回転オフセット", &offsetRot.x, 0.01f, -DirectX::XM_PI, DirectX::XM_PI);

	ImGui::Separator();
	ImGui::TextColored(ImVec4(0, 1, 1, 1), u8"▼ アニメーション解析・テスト");
	Model* model = (cachedModelComponent) ? cachedModelComponent->GetModel().get() : nullptr;
	if (model && animator)
	{
		ImGui::InputText(u8"現在のアニメ名", currentAnimName, sizeof(currentAnimName));
		if (ImGui::Button(u8"この名前で再生テスト"))
		{
			int animIdx = model->GetAnimationIndex(currentAnimName);
			if (animIdx >= 0) animator->Play(animIdx, 0.1f, true);
		}
	}
}