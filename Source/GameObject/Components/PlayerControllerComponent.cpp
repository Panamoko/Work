#include <algorithm>
#include <imgui.h>
#include <fstream>
#include <cmath> 
#include "GameObject/Components/PlayerControllerComponent.h"
#include "Core/WindowUtils.h"
#include "Graphics/Graphics.h"
#include "Core/Input.h"
#include "GameObject/Actor.h"

PlayerControllerComponent::PlayerControllerComponent(Camera* camera, FpsCameraController* cameraController, const CollisionMesh& collisionMesh)
	: camera(camera), cameraController(cameraController), collisionMesh(collisionMesh) {
	this->collisionMesh.BuildSpatialGrid();
}

void PlayerControllerComponent::Start() {
	cachedModelComponent = GetActor()->GetComponent<ModelComponent>().get();
	animator = GetActor()->GetComponent<AnimatorComponent>().get();
	LoadSettings();
}

void PlayerControllerComponent::SaveSettings() {
	std::ofstream ofs("PlayerSettings.txt");
	if (ofs) {
		ofs << playerScale << "\n";
		ofs << cameraOffsetPos.x << " " << cameraOffsetPos.y << " " << cameraOffsetPos.z << "\n";
		ofs << radius << "\n";
		ofs << manualMoveSpeed << " " << runMoveSpeed << "\n";
		ofs << gravity << " " << jumpPower << "\n";
		ofs << acceleration << " " << deceleration << " " << airControl << "\n";
	}
}

void PlayerControllerComponent::LoadSettings() {
	std::ifstream ifs("PlayerSettings.txt");
	if (ifs) {
		ifs >> playerScale;
		ifs >> cameraOffsetPos.x >> cameraOffsetPos.y >> cameraOffsetPos.z;
		ifs >> radius;
		float tempSpeed, tempRunSpeed, tempGravity, tempJump, tempAccel, tempDecel, tempAir;
		if (ifs >> tempSpeed >> tempRunSpeed) {
			manualMoveSpeed = tempSpeed;
			runMoveSpeed = tempRunSpeed;
		}
		if (ifs >> tempGravity >> tempJump) {
			gravity = tempGravity;
			jumpPower = tempJump;
		}
		if (ifs >> tempAccel >> tempDecel >> tempAir) {
			acceleration = tempAccel;
			deceleration = tempDecel;
			airControl = tempAir;
		}
	}
}

bool PlayerControllerComponent::CheckGrounded() {
	DirectX::XMVECTOR Origin = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&position), DirectX::XMVectorSet(0.0f, radius + stepOffset, 0.0f, 0.0f));
	DirectX::XMFLOAT3 originF; DirectX::XMStoreFloat3(&originF, Origin);
	DirectX::XMFLOAT3 hitPos, hitNormal;
	float checkDistance = stepOffset + skinWidth + 0.1f;
	return collisionMesh.SphereCast(originF, { 0.0f, -1.0f, 0.0f }, radius, checkDistance, hitPos, hitNormal);
}


void PlayerControllerComponent::OnGUI()
{
	// ---- Top bar ----
	if (ImGui::Button("Save Settings")) SaveSettings();
	ImGui::SameLine();
	if (ImGui::Button("Load Settings")) LoadSettings();
	ImGui::TextDisabled("Keys:  F11 = Fullscreen   /   TAB = Toggle cursor");
	ImGui::Spacing();

	// ---- Movement ----
	if (ImGui::CollapsingHeader("Movement", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat("Walk Speed", &manualMoveSpeed, 0.1f, 0.0f, 30.0f);
		ImGui::DragFloat("Run Speed", &runMoveSpeed, 0.1f, 0.0f, 40.0f);
		ImGui::Checkbox("Use Physics (gravity / collision)", &usePhysics);
	}

	// ---- Jump & Gravity ----
	if (ImGui::CollapsingHeader("Jump & Gravity", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat("Jump Power", &jumpPower, 0.1f, 0.0f, 50.0f);
		ImGui::DragFloat("Gravity", &gravity, 0.1f, 0.0f, 100.0f);
	}

	// ---- Feel (Inertia) ----
	if (ImGui::CollapsingHeader("Feel (Inertia)", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat("Acceleration", &acceleration, 0.1f, 1.0f, 50.0f);
		ImGui::DragFloat("Deceleration", &deceleration, 0.1f, 1.0f, 50.0f);
		ImGui::SliderFloat("Air Control", &airControl, 0.0f, 1.0f);
	}

	// ---- Camera ----
	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Eye Offset (X,Y,Z)", &cameraOffsetPos.x, 0.01f, -5.0f, 5.0f);
		ImGui::SliderFloat("Field of View", &currentFov, 30.0f, 120.0f, "%.0f deg");
		ImGui::DragFloat("Mouse Sensitivity", &mouseSensitivity, 0.0001f, 0.0f, 0.02f, "%.4f");
		ImGui::Checkbox("Lock Cursor", &isCursorLocked);
	}

	// ---- Collision / Capsule ----
	if (ImGui::CollapsingHeader("Collision"))
	{
		ImGui::DragFloat("Capsule Radius", &radius, 0.01f, 0.01f, 2.0f);
		ImGui::DragFloat("Skin Width", &skinWidth, 0.001f, 0.0f, 0.5f, "%.3f");
		ImGui::DragFloat("Step Offset", &stepOffset, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Slope Limit (deg)", &slopeLimit, 0.5f, 0.0f, 89.0f);
		ImGui::DragFloat("Player Scale", &playerScale, 0.01f, 0.05f, 10.0f);
	}

	// ---- Animation ----
	if (ImGui::CollapsingHeader("Animation"))
	{
		ImGui::DragFloat("Anim Speed", &animSpeed, 0.01f, 0.0f, 5.0f);
		ImGui::DragFloat("Cross Fade Time", &crossFadeTime, 0.01f, 0.0f, 1.0f);
		ImGui::DragFloat("Fall Clamp (sec)", &fallClampSeconds, 0.05f, 0.0f, 5.0f);
		ImGui::Separator();
		ImGui::TextDisabled("Animation clip names");
		ImGui::InputText("Idle", animIdle, sizeof(animIdle));
		ImGui::InputText("Walk Fwd", animWalkF, sizeof(animWalkF));
		ImGui::InputText("Walk Back", animWalkB, sizeof(animWalkB));
		ImGui::InputText("Walk Left", animWalkL, sizeof(animWalkL));
		ImGui::InputText("Walk Right", animWalkR, sizeof(animWalkR));
		ImGui::InputText("Run", animRun, sizeof(animRun));
		ImGui::InputText("Jump", animJump, sizeof(animJump));
		ImGui::InputText("Fall", animFall, sizeof(animFall));
	}

	// ---- Runtime State (read-only monitor) ----
	if (ImGui::CollapsingHeader("Runtime State", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const char* stateNames[] = { "Idle", "Walk", "Run", "Jump", "Fall" };
		int s = static_cast<int>(currentState);
		ImGui::Text("State     : %s", (s >= 0 && s < 5) ? stateNames[s] : "?");
		ImGui::TextColored(isGrounded ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0.5f, 0, 1),
			"Grounded  : %s", isGrounded ? "YES" : "NO (airborne)");
		ImGui::Text("Position  : %.2f, %.2f, %.2f", position.x, position.y, position.z);
		ImGui::Text("Velocity  : %.2f, %.2f, %.2f", currentVelocity.x, currentVelocity.y, currentVelocity.z);
		ImGui::Text("Vertical V: %.2f", velocityY);
		ImGui::Text("Yaw/Pitch : %.1f / %.1f", characterYaw, cameraPitch);
	}
}

void PlayerControllerComponent::Update(float elapsedTime) {
	HWND hWnd = GetActiveWindow();

	bool currF11Key = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
	if (currF11Key && !prevF11Key) {
		WindowUtils::ToggleFullscreen(hWnd, isFullscreen, savedWindowRect);
	}
	prevF11Key = currF11Key;

	Model* characterModel = nullptr;
	if (cachedModelComponent && cachedModelComponent->GetModel()) {
		characterModel = cachedModelComponent->GetModel().get();
	}

	bool currTabKey = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
	if (currTabKey && !prevTabKey) isCursorLocked = !isCursorLocked;
	prevTabKey = currTabKey;

	float mouseDX = 0.0f, mouseDY = 0.0f;
	WindowUtils::UpdateCursorLock(hWnd, isCursorLocked, cursorVisibilityState,
		Graphics::Instance().GetScreenWidth(), Graphics::Instance().GetScreenHeight(), mouseDX, mouseDY);
	characterYaw += mouseDX * mouseSensitivity;
	cameraPitch += mouseDY * mouseSensitivity;
	cameraPitch = std::clamp(cameraPitch, -DirectX::XM_PIDIV2 + 0.05f, DirectX::XM_PIDIV2 - 0.05f);

	DirectX::XMFLOAT3 manualVec = { 0, 0, 0 };
	GamePad& pad = Input::Instance().GetGamePad();
	float inputX = pad.GetAxisLX(), inputZ = pad.GetAxisLY();

	if (GetAsyncKeyState('W')) inputZ = 1.0f;
	if (GetAsyncKeyState('S')) inputZ = -1.0f;
	if (GetAsyncKeyState('A')) inputX = -1.0f;
	if (GetAsyncKeyState('D')) inputX = 1.0f;

	bool isShiftPressed = ((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0) || (pad.GetTriggerL() > 0.5f);

	DirectX::XMVECTOR vInput = DirectX::XMVectorSet(inputX, 0.0f, inputZ, 0.0f);
	DirectX::XMVECTOR vInputLengthSq = DirectX::XMVector3LengthSq(vInput);
	bool currentlyMoving = (vInputLengthSq.m128_f32[0] > 0.001f);

	if (currentlyMoving) {
		if (vInputLengthSq.m128_f32[0] > 1.0f) vInput = DirectX::XMVector3Normalize(vInput);
		DirectX::XMStoreFloat3(&manualVec, vInput);
	}

	if (usePhysics) isGrounded = (velocityY > 0.0f) ? false : CheckGrounded();
	else isGrounded = true;

	bool currSpaceKey = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
	bool isSpacePressed = currSpaceKey && !prevSpaceKey;

	PlayerState nextState = currentState;
	if (isGrounded) {
		if (currentState == PlayerState::Jump || currentState == PlayerState::Fall) velocityY = 0.0f;

		if ((pad.GetButtonDown() & GamePad::BTN_A) || isSpacePressed) {
			velocityY = jumpPower; nextState = PlayerState::Jump;
		}
		else if (currentlyMoving) {
			nextState = isShiftPressed ? PlayerState::Run : PlayerState::Walk;
		}
		else {
			nextState = PlayerState::Idle;
		}
	}
	else {
		nextState = (velocityY > 0.0f) ? PlayerState::Jump : PlayerState::Fall;
	}

	prevSpaceKey = currSpaceKey;
	bool stateChanged = (currentState != nextState);
	currentState = nextState;

	UpdateAnimation(characterModel, stateChanged, manualVec);

	// =======================================================
	// ★プロの修正1：空中でもShiftキーを基準に目標速度を決定する
	// =======================================================
	float currentMoveSpeed = isShiftPressed ? runMoveSpeed : manualMoveSpeed;

	DirectX::XMVECTOR vInputLocal = DirectX::XMVectorSet(manualVec.x, 0.0f, manualVec.z, 0.0f);
	DirectX::XMMATRIX R_Yaw = DirectX::XMMatrixRotationY(characterYaw);
	DirectX::XMVECTOR vTargetVelocity = DirectX::XMVector3TransformNormal(vInputLocal, R_Yaw);
	vTargetVelocity = DirectX::XMVectorScale(vTargetVelocity, currentMoveSpeed);

	float accelRate = currentlyMoving ? acceleration : deceleration;
	if (!isGrounded) {
		accelRate *= airControl;
	}

	DirectX::XMVECTOR vCurrentVelocity = DirectX::XMLoadFloat3(&currentVelocity);
	float t = 1.0f - std::exp(-accelRate * elapsedTime);
	vCurrentVelocity = DirectX::XMVectorLerp(vCurrentVelocity, vTargetVelocity, t);
	DirectX::XMStoreFloat3(&currentVelocity, vCurrentVelocity);

	DirectX::XMVECTOR vWorldTranslation = DirectX::XMVectorScale(vCurrentVelocity, elapsedTime);
	DirectX::XMFLOAT3 move; DirectX::XMStoreFloat3(&move, vWorldTranslation);

	if (usePhysics) {
		if (!isGrounded) velocityY -= gravity * elapsedTime;
		move.y = velocityY * elapsedTime;
		MoveAndSlide({ move.x, 0, move.z }, false);
		MoveAndSlide({ 0, move.y, 0 }, true);
	}
	else {
		position.x += move.x; position.z += move.z;
	}

	GetActor()->SetPosition(position);
	DirectX::XMFLOAT4 rot;
	DirectX::XMStoreFloat4(&rot, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, characterYaw, 0.0f));
	GetActor()->SetRotation(rot);
	GetActor()->SetScale({ playerScale, playerScale, playerScale });

	UpdateCameraFollow(vCurrentVelocity, elapsedTime);
}

bool PlayerControllerComponent::CapsuleCast(const DirectX::XMFLOAT3& originFoot, const DirectX::XMFLOAT3& originHead, const DirectX::XMFLOAT3& direction, float radius, float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal) {
	float distFoot = distance;
	float distHead = distance;
	DirectX::XMFLOAT3 hitPosFoot, hitNormFoot, hitPosHead, hitNormHead;

	bool hitFoot = collisionMesh.SphereCast(originFoot, direction, radius, distFoot, hitPosFoot, hitNormFoot);
	bool hitHead = collisionMesh.SphereCast(originHead, direction, radius, distHead, hitPosHead, hitNormHead);

	if (hitFoot || hitHead) {
		if (hitFoot && hitHead) {
			if (distFoot < distHead) {
				distance = distFoot; hitPosition = hitPosFoot; hitNormal = hitNormFoot;
			}
			else {
				distance = distHead; hitPosition = hitPosHead; hitNormal = hitNormHead;
			}
		}
		else if (hitFoot) {
			distance = distFoot; hitPosition = hitPosFoot; hitNormal = hitNormFoot;
		}
		else {
			distance = distHead; hitPosition = hitPosHead; hitNormal = hitNormHead;
		}
		return true;
	}
	return false;
}

void PlayerControllerComponent::MoveAndSlide(const DirectX::XMFLOAT3& move, bool vertical) {
	DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&position), Move = DirectX::XMLoadFloat3(&move);

	DirectX::XMVECTOR CenterFoot = DirectX::XMVectorSet(0.0f, radius + stepOffset, 0.0f, 0.0f);
	DirectX::XMVECTOR CenterHead = DirectX::XMVectorSet(0.0f, cameraOffsetPos.y, 0.0f, 0.0f);

	for (int i = 0; i < 3; ++i) {
		float moveDist = DirectX::XMVectorGetX(DirectX::XMVector3Length(Move));
		if (moveDist < 0.0001f) break;
		float castDist = moveDist + skinWidth;
		if (vertical) castDist += stepOffset;

		DirectX::XMVECTOR OriginFoot = DirectX::XMVectorAdd(Position, CenterFoot);
		DirectX::XMVECTOR OriginHead = DirectX::XMVectorAdd(Position, CenterHead);
		DirectX::XMVECTOR Direction = DirectX::XMVector3Normalize(Move);

		DirectX::XMFLOAT3 originFootF, originHeadF, directionF, hitPosition, hitNormal;
		DirectX::XMStoreFloat3(&originFootF, OriginFoot);
		DirectX::XMStoreFloat3(&originHeadF, OriginHead);
		DirectX::XMStoreFloat3(&directionF, Direction);

		if (CapsuleCast(originFootF, originHeadF, directionF, radius, castDist, hitPosition, hitNormal)) {
			castDist -= skinWidth;
			if (vertical) castDist -= stepOffset;
			if (castDist < 0.0f) castDist = 0.0f;

			DirectX::XMVECTOR Vec = DirectX::XMVectorScale(Direction, castDist);
			Position = DirectX::XMVectorAdd(Position, Vec);

			DirectX::XMVECTOR HitNormal = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&hitNormal)), Up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			float dot = std::clamp(DirectX::XMVectorGetX(DirectX::XMVector3Dot(HitNormal, Up)), -1.0f, 1.0f);
			if (DirectX::XMConvertToDegrees(acosf(dot)) <= slopeLimit) break;

			Move = DirectX::XMVectorSubtract(Move, Vec);
			DirectX::XMVECTOR dotVec = DirectX::XMVector3Dot(Move, HitNormal);
			if (DirectX::XMVectorGetX(dotVec) < 0.0f) {
				Move = DirectX::XMVectorSubtract(Move, DirectX::XMVectorMultiply(HitNormal, dotVec));
			}
			if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Move)) < 0.00001f) break;
		}
		else {
			Position = DirectX::XMVectorAdd(Position, Move);
			break;
		}
	}
	DirectX::XMStoreFloat3(&position, Position);
}


bool PlayerControllerComponent::RayCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance, float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal) {
	return collisionMesh.RayCast(origin, direction, maxDistance, distance, hitPosition, hitNormal);
}

void PlayerControllerComponent::UpdateAnimation(Model* characterModel, bool stateChanged, const DirectX::XMFLOAT3& manualVec) {
	if (!(animator && characterModel)) return;

	animator->SetAnimSpeed(animSpeed);
	if (currentState == PlayerState::Walk || currentState == PlayerState::Run) {

		int animZ = currentAnimZIndex;
		if (currentState == PlayerState::Run) {
			animZ = characterModel->GetAnimationIndex(animRun);
			if (animZ < 0) animZ = characterModel->GetAnimationIndex(animWalkF);
		}
		else {
			if (manualVec.z > 0.05f) animZ = characterModel->GetAnimationIndex(animWalkF);
			else if (manualVec.z < -0.05f) animZ = characterModel->GetAnimationIndex(animWalkB);
			if (animZ < 0) animZ = characterModel->GetAnimationIndex(animWalkF);
		}

		int animX = currentAnimXIndex;
		if (manualVec.x > 0.05f) animX = characterModel->GetAnimationIndex(animWalkR);
		else if (manualVec.x < -0.05f) animX = characterModel->GetAnimationIndex(animWalkL);
		if (animX < 0) animX = characterModel->GetAnimationIndex(animWalkR);

		float weightZ = std::abs(manualVec.z), weightX = std::abs(manualVec.x);
		float ratio = (weightZ + weightX > 0.001f) ? (weightX / (weightZ + weightX)) : 0.0f;

		if (stateChanged || animZ != currentAnimZIndex || animX != currentAnimXIndex) {
			currentAnimZIndex = animZ; currentAnimXIndex = animX;
			animator->PlayBlend(animZ, animX, ratio, crossFadeTime, true);
		}
		else {
			animator->UpdateBlendRatio(ratio);
		}
	}
	else {
		if (stateChanged) {
			currentAnimZIndex = currentAnimXIndex = -1;
			int animIdx = -1; bool loop = true; float clampTime = -1.0f;
			switch (currentState) {
			case PlayerState::Idle: animIdx = characterModel->GetAnimationIndex(animIdle); loop = true; break;
			case PlayerState::Jump: animIdx = characterModel->GetAnimationIndex(animJump); loop = false; break;
			case PlayerState::Fall: animIdx = characterModel->GetAnimationIndex(animFall); loop = false; clampTime = fallClampSeconds; break;
			}
			if (animIdx < 0) animIdx = characterModel->GetAnimationIndex(animIdle);
			animator->Play(animIdx, crossFadeTime, loop, clampTime);
		}
	}
}

void PlayerControllerComponent::UpdateCameraFollow(DirectX::FXMVECTOR currentVelocity, float elapsedTime) {
	DirectX::XMVECTOR vEyePos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&position), DirectX::XMLoadFloat3(&cameraOffsetPos));

	DirectX::XMMATRIX R_Camera = DirectX::XMMatrixRotationRollPitchYaw(cameraPitch, characterYaw, 0.0f);
	DirectX::XMVECTOR vTargetDirection = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), R_Camera);
	DirectX::XMVECTOR vTargetPos = DirectX::XMVectorAdd(vEyePos, vTargetDirection);

	DirectX::XMFLOAT3 eyeF, targetF;
	DirectX::XMStoreFloat3(&eyeF, vEyePos); DirectX::XMStoreFloat3(&targetF, vTargetPos);

	camera->SetLookAt(eyeF, targetF, { 0.0f, 1.0f, 0.0f });

	float screenW = Graphics::Instance().GetScreenWidth();
	float screenH = Graphics::Instance().GetScreenHeight();
	if (screenW > 0 && screenH > 0) {
		// Speed-based FOV: scales from 60 (idle) toward 75 (run) using the actual
		// physical velocity, so the widened FOV holds during airborne inertia.
		float currentSpeed = DirectX::XMVectorGetX(DirectX::XMVector3Length(currentVelocity));
		float speedRatio = std::clamp((currentSpeed - manualMoveSpeed) / (runMoveSpeed - manualMoveSpeed), 0.0f, 1.0f);
		float targetFov = 60.0f + (15.0f * speedRatio);
		currentFov += (targetFov - currentFov) * 10.0f * elapsedTime;
		camera->SetPerspectiveFov(DirectX::XMConvertToRadians(currentFov), screenW / screenH, 0.05f, 1000.0f);
	}
}
