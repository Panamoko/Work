#include <algorithm>
#include <imgui.h>
#include <fstream>
#include <cmath> 
#include "GameObject/Components/PlayerControllerComponent.h"
#include "Physics/SphereCast.h"
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
	return SphereCast(originF, { 0.0f, -1.0f, 0.0f }, radius, checkDistance, hitPos, hitNormal);
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
	if (currF11Key && !prevF11Key && hWnd != nullptr) {
		if (!isFullscreen) {
			GetWindowRect(hWnd, &savedWindowRect);
			SetWindowLong(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
			HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFO mi; mi.cbSize = sizeof(mi); GetMonitorInfo(hMonitor, &mi);
			SetWindowPos(hWnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			isFullscreen = true;
		}
		else {
			SetWindowLong(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
			SetWindowPos(hWnd, nullptr, savedWindowRect.left, savedWindowRect.top, savedWindowRect.right - savedWindowRect.left, savedWindowRect.bottom - savedWindowRect.top, SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
			isFullscreen = false;
		}
	}
	prevF11Key = currF11Key;

	Model* characterModel = nullptr;
	if (cachedModelComponent && cachedModelComponent->GetModel()) {
		characterModel = cachedModelComponent->GetModel().get();
	}

	bool currTabKey = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
	if (currTabKey && !prevTabKey) isCursorLocked = !isCursorLocked;
	prevTabKey = currTabKey;

	if (hWnd == GetForegroundWindow()) {
		if (isCursorLocked) {
			if (cursorVisibilityState) { ShowCursor(FALSE); cursorVisibilityState = false; }
			float screenW = Graphics::Instance().GetScreenWidth(); float screenH = Graphics::Instance().GetScreenHeight();
			POINT center = { static_cast<LONG>(screenW / 2.0f), static_cast<LONG>(screenH / 2.0f) };
			ClientToScreen(hWnd, &center);
			POINT currentMousePos; GetCursorPos(&currentMousePos);
			characterYaw += static_cast<float>(currentMousePos.x - center.x) * mouseSensitivity;
			cameraPitch += static_cast<float>(currentMousePos.y - center.y) * mouseSensitivity;
			cameraPitch = std::clamp(cameraPitch, -DirectX::XM_PIDIV2 + 0.05f, DirectX::XM_PIDIV2 - 0.05f);
			SetCursorPos(center.x, center.y);
		}
		else {
			if (!cursorVisibilityState) { ShowCursor(TRUE); cursorVisibilityState = true; }
		}
	}

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

	if (animator && characterModel) {
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

		// =======================================================
		// ★プロの修正2：実際の「物理的な移動スピード(Velocity)」からFOVを計算する！
		// これにより、空中に飛び出して慣性でハイスピード移動している間は絶対にFOVが維持される。
		// =======================================================
		float currentSpeed = DirectX::XMVectorGetX(DirectX::XMVector3Length(vCurrentVelocity));

		// 速度0でFOV60、runMoveSpeedでFOV75になるように割合を計算（clampで0.0〜1.0に制限）
		float speedRatio = std::clamp((currentSpeed - manualMoveSpeed) / (runMoveSpeed - manualMoveSpeed), 0.0f, 1.0f);
		float targetFov = 60.0f + (15.0f * speedRatio);

		currentFov += (targetFov - currentFov) * 10.0f * elapsedTime;
		camera->SetPerspectiveFov(DirectX::XMConvertToRadians(currentFov), screenW / screenH, 0.05f, 1000.0f);
	}
}

bool PlayerControllerComponent::CapsuleCast(const DirectX::XMFLOAT3& originFoot, const DirectX::XMFLOAT3& originHead, const DirectX::XMFLOAT3& direction, float radius, float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal) {
	float distFoot = distance;
	float distHead = distance;
	DirectX::XMFLOAT3 hitPosFoot, hitNormFoot, hitPosHead, hitNormHead;

	bool hitFoot = SphereCast(originFoot, direction, radius, distFoot, hitPosFoot, hitNormFoot);
	bool hitHead = SphereCast(originHead, direction, radius, distHead, hitPosHead, hitNormHead);

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

bool PlayerControllerComponent::SphereCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float radius, float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal) {
	bool hit = false;
	DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&origin), Direction = DirectX::XMLoadFloat3(&direction);
	DirectX::XMVECTOR End = DirectX::XMVectorAdd(Start, DirectX::XMVectorScale(Direction, distance));

	float minX = (std::min)(origin.x, origin.x + direction.x * distance) - radius;
	float minY = (std::min)(origin.y, origin.y + direction.y * distance) - radius;
	float minZ = (std::min)(origin.z, origin.z + direction.z * distance) - radius;
	float maxX = (std::max)(origin.x, origin.x + direction.x * distance) + radius;
	float maxY = (std::max)(origin.y, origin.y + direction.y * distance) + radius;
	float maxZ = (std::max)(origin.z, origin.z + direction.z * distance) + radius;

	int startX = static_cast<int>(std::floor(minX / collisionMesh.cellSize));
	int startY = static_cast<int>(std::floor(minY / collisionMesh.cellSize));
	int startZ = static_cast<int>(std::floor(minZ / collisionMesh.cellSize));
	int endX = static_cast<int>(std::floor(maxX / collisionMesh.cellSize));
	int endY = static_cast<int>(std::floor(maxY / collisionMesh.cellSize));
	int endZ = static_cast<int>(std::floor(maxZ / collisionMesh.cellSize));

	std::vector<size_t> candidateIndices;
	for (int x = startX; x <= endX; ++x) {
		for (int y = startY; y <= endY; ++y) {
			for (int z = startZ; z <= endZ; ++z) {
				auto it = collisionMesh.gridCells.find(collisionMesh.ComputeHash(x, y, z));
				if (it != collisionMesh.gridCells.end()) {
					candidateIndices.insert(candidateIndices.end(), it->second.begin(), it->second.end());
				}
			}
		}
	}

	std::sort(candidateIndices.begin(), candidateIndices.end());
	candidateIndices.erase(std::unique(candidateIndices.begin(), candidateIndices.end()), candidateIndices.end());

	for (size_t idx : candidateIndices) {
		const auto& triangle = collisionMesh.triangles[idx];
		DirectX::XMVECTOR Positions[3] = { DirectX::XMLoadFloat3(&triangle.positions[0]), DirectX::XMLoadFloat3(&triangle.positions[1]), DirectX::XMLoadFloat3(&triangle.positions[2]) };
		SphereCastResult result;
		if (IntersectSphereCastVsTriangle(Start, End, radius, Positions, &result)) {
			if (distance < result.distance) continue;
			distance = result.distance;
			DirectX::XMStoreFloat3(&hitPosition, DirectX::XMVectorAdd(Start, DirectX::XMVectorScale(Direction, distance)));
			DirectX::XMStoreFloat3(&hitNormal, result.normal);
			hit = true;
		}
	}
	return hit;
}

bool PlayerControllerComponent::RayCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float maxDistance, float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal) {
	bool hit = false;
	distance = maxDistance;
	DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&origin);
	DirectX::XMVECTOR Dir = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&direction));

	float minX = (std::min)(origin.x, origin.x + direction.x * maxDistance);
	float minY = (std::min)(origin.y, origin.y + direction.y * maxDistance);
	float minZ = (std::min)(origin.z, origin.z + direction.z * maxDistance);
	float maxX = (std::max)(origin.x, origin.x + direction.x * maxDistance);
	float maxY = (std::max)(origin.y, origin.y + direction.y * maxDistance);
	float maxZ = (std::max)(origin.z, origin.z + direction.z * maxDistance);

	int startX = static_cast<int>(std::floor(minX / collisionMesh.cellSize));
	int startY = static_cast<int>(std::floor(minY / collisionMesh.cellSize));
	int startZ = static_cast<int>(std::floor(minZ / collisionMesh.cellSize));
	int endX = static_cast<int>(std::floor(maxX / collisionMesh.cellSize));
	int endY = static_cast<int>(std::floor(maxY / collisionMesh.cellSize));
	int endZ = static_cast<int>(std::floor(maxZ / collisionMesh.cellSize));

	std::vector<size_t> candidateIndices;
	for (int x = startX; x <= endX; ++x) {
		for (int y = startY; y <= endY; ++y) {
			for (int z = startZ; z <= endZ; ++z) {
				auto it = collisionMesh.gridCells.find(collisionMesh.ComputeHash(x, y, z));
				if (it != collisionMesh.gridCells.end()) {
					candidateIndices.insert(candidateIndices.end(), it->second.begin(), it->second.end());
				}
			}
		}
	}

	std::sort(candidateIndices.begin(), candidateIndices.end());
	candidateIndices.erase(std::unique(candidateIndices.begin(), candidateIndices.end()), candidateIndices.end());

	const float EPSILON = 0.0000001f;
	for (size_t idx : candidateIndices) {
		const auto& triangle = collisionMesh.triangles[idx];
		DirectX::XMVECTOR v0 = DirectX::XMLoadFloat3(&triangle.positions[0]);
		DirectX::XMVECTOR v1 = DirectX::XMLoadFloat3(&triangle.positions[1]);
		DirectX::XMVECTOR v2 = DirectX::XMLoadFloat3(&triangle.positions[2]);

		DirectX::XMVECTOR edge1 = DirectX::XMVectorSubtract(v1, v0);
		DirectX::XMVECTOR edge2 = DirectX::XMVectorSubtract(v2, v0);
		DirectX::XMVECTOR h = DirectX::XMVector3Cross(Dir, edge2);
		float a = DirectX::XMVectorGetX(DirectX::XMVector3Dot(edge1, h));
		if (a > -EPSILON && a < EPSILON) continue;

		float f = 1.0f / a;
		DirectX::XMVECTOR s = DirectX::XMVectorSubtract(Start, v0);
		float u = f * DirectX::XMVectorGetX(DirectX::XMVector3Dot(s, h));
		if (u < 0.0f || u > 1.0f) continue;

		DirectX::XMVECTOR q = DirectX::XMVector3Cross(s, edge1);
		float v = f * DirectX::XMVectorGetX(DirectX::XMVector3Dot(Dir, q));
		if (v < 0.0f || u + v > 1.0f) continue;

		float t = f * DirectX::XMVectorGetX(DirectX::XMVector3Dot(edge2, q));

		if (t > EPSILON && t < distance) {
			distance = t;
			DirectX::XMStoreFloat3(&hitNormal, DirectX::XMVector3Normalize(DirectX::XMVector3Cross(edge1, edge2)));
			hit = true;
		}
	}

	if (hit) {
		DirectX::XMStoreFloat3(&hitPosition, DirectX::XMVectorAdd(Start, DirectX::XMVectorScale(Dir, distance)));
	}
	return hit;
}