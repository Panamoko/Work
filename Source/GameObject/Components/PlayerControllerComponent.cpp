#include <algorithm>
#include <imgui.h>
#include "GameObject/Components/PlayerControllerComponent.h"
#include "Physics/SphereCast.h"
#include "Graphics/Graphics.h"
#include "Core/Input.h"
#include "GameObject/Actor.h"

PlayerControllerComponent::PlayerControllerComponent(Camera* camera, FpsCameraController* cameraController, const CollisionMesh& collisionMesh, Actor* weaponActor)
	: camera(camera), cameraController(cameraController), collisionMesh(collisionMesh), weaponActor(weaponActor)
{
}

void PlayerControllerComponent::Start()
{
	cachedModelComponent = GetActor()->GetComponent<ModelComponent>().get();
	animator = GetActor()->GetComponent<AnimatorComponent>().get();
}

bool PlayerControllerComponent::CheckGrounded()
{
	DirectX::XMVECTOR Origin = DirectX::XMVectorAdd(
		DirectX::XMLoadFloat3(&position),
		DirectX::XMVectorSet(0.0f, radius + stepOffset, 0.0f, 0.0f)
	);
	DirectX::XMFLOAT3 originF; DirectX::XMStoreFloat3(&originF, Origin);
	DirectX::XMFLOAT3 hitPos, hitNormal;
	float checkDistance = stepOffset + skinWidth + 0.1f;
	return SphereCast(originF, { 0.0f, -1.0f, 0.0f }, radius, checkDistance, hitPos, hitNormal);
}

void PlayerControllerComponent::Update(float elapsedTime)
{
	if (!cachedModelComponent) return;
	Model* characterModel = cachedModelComponent->GetModel().get();
	if (!characterModel) return;

	// =======================================================
	// 0. プロのFPS技術：[TAB]キーによるカーソルロック切り替え
	// =======================================================
	// TABキーが押された瞬間を検知してロック状態を反転
	bool currTabKey = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0;
	if (currTabKey && !prevTabKey)
	{
		isCursorLocked = !isCursorLocked;
	}
	prevTabKey = currTabKey;

	HWND hWnd = GetActiveWindow();
	if (hWnd == GetForegroundWindow())
	{
		if (isCursorLocked)
		{
			// カーソルがまだ表示されていれば隠す
			if (cursorVisibilityState) { ShowCursor(FALSE); cursorVisibilityState = false; }

			float screenW = Graphics::Instance().GetScreenWidth();
			float screenH = Graphics::Instance().GetScreenHeight();
			POINT center = { static_cast<LONG>(screenW / 2.0f), static_cast<LONG>(screenH / 2.0f) };
			ClientToScreen(hWnd, &center);

			POINT currentMousePos;
			GetCursorPos(&currentMousePos);

			float deltaX = static_cast<float>(currentMousePos.x - center.x);
			float deltaY = static_cast<float>(currentMousePos.y - center.y);

			characterYaw += deltaX * mouseSensitivity;
			cameraPitch += deltaY * mouseSensitivity;
			cameraPitch = std::clamp(cameraPitch, -DirectX::XM_PIDIV2 + 0.05f, DirectX::XM_PIDIV2 - 0.05f);

			SetCursorPos(center.x, center.y);
		}
		else
		{
			// ロック解除時：カーソルを再び表示する
			if (!cursorVisibilityState) { ShowCursor(TRUE); cursorVisibilityState = true; }
		}
	}

	DirectX::XMFLOAT3 manualVec = { 0, 0, 0 };
	GamePad& pad = Input::Instance().GetGamePad();
	float inputX = pad.GetAxisLX();
	float inputZ = pad.GetAxisLY();

	if (GetAsyncKeyState('W')) inputZ = 1.0f;
	if (GetAsyncKeyState('S')) inputZ = -1.0f;
	if (GetAsyncKeyState('A')) inputX = -1.0f;
	if (GetAsyncKeyState('D')) inputX = 1.0f;

	DirectX::XMVECTOR vInput = DirectX::XMVectorSet(inputX, 0.0f, inputZ, 0.0f);
	DirectX::XMVECTOR vInputLengthSq = DirectX::XMVector3LengthSq(vInput);
	bool currentlyMoving = (vInputLengthSq.m128_f32[0] > 0.001f);

	if (currentlyMoving)
	{
		if (vInputLengthSq.m128_f32[0] > 1.0f) vInput = DirectX::XMVector3Normalize(vInput);
		DirectX::XMStoreFloat3(&manualVec, vInput);
	}

	if (usePhysics)
	{
		if (velocityY > 0.0f) isGrounded = false;
		else isGrounded = CheckGrounded();
	}
	else isGrounded = true;

	PlayerState nextState = currentState;

	if (isGrounded)
	{
		if (currentState == PlayerState::Jump || currentState == PlayerState::Fall) velocityY = 0.0f;
		if ((pad.GetButtonDown() & GamePad::BTN_A) || GetAsyncKeyState(VK_SPACE))
		{
			velocityY = jumpPower;
			nextState = PlayerState::Jump;
		}
		else nextState = currentlyMoving ? PlayerState::Walk : PlayerState::Idle;
	}
	else
	{
		if (velocityY > 0.0f) nextState = PlayerState::Jump;
		else                  nextState = PlayerState::Fall;
	}

	bool stateChanged = (currentState != nextState);
	currentState = nextState;

	if (animator)
	{
		animator->SetAnimSpeed(animSpeed);
		if (currentState == PlayerState::Walk)
		{
			int animZ = characterModel->GetAnimationIndex((manualVec.z >= 0.0f) ? animWalkF : animWalkB);
			int animX = characterModel->GetAnimationIndex((manualVec.x >= 0.0f) ? animWalkR : animWalkL);

			float weightZ = std::abs(manualVec.z);
			float weightX = std::abs(manualVec.x);
			float ratio = (weightZ + weightX > 0.001f) ? (weightX / (weightZ + weightX)) : 0.0f;

			if (stateChanged || animZ != currentAnimZIndex || animX != currentAnimXIndex)
			{
				currentAnimZIndex = animZ;
				currentAnimXIndex = animX;
				animator->PlayBlend(animZ, animX, ratio, crossFadeTime, true);
			}
			else animator->UpdateBlendRatio(ratio);
		}
		else
		{
			if (stateChanged)
			{
				currentAnimZIndex = -1;
				currentAnimXIndex = -1;
				int animIdx = -1;
				bool loop = true;
				float clampTime = -1.0f;

				switch (currentState)
				{
				case PlayerState::Idle: animIdx = characterModel->GetAnimationIndex(animIdle); loop = true; break;
				case PlayerState::Jump: animIdx = characterModel->GetAnimationIndex(animJump); loop = false; break;
				case PlayerState::Fall: animIdx = characterModel->GetAnimationIndex(animFall); loop = false; clampTime = fallClampSeconds; break;
				}

				if (animIdx < 0) animIdx = characterModel->GetAnimationIndex(animIdle);
				animator->Play(animIdx, crossFadeTime, loop, clampTime);
			}
		}
	}

	DirectX::XMVECTOR vManualTranslation = DirectX::XMVectorSet(manualVec.x * manualMoveSpeed * elapsedTime, 0.0f, manualVec.z * manualMoveSpeed * elapsedTime, 0.0f);
	DirectX::XMMATRIX R_Yaw = DirectX::XMMatrixRotationY(characterYaw);
	DirectX::XMVECTOR vWorldTranslation = DirectX::XMVector3TransformNormal(vManualTranslation, R_Yaw);
	DirectX::XMStoreFloat3(&debugManualMotion, vManualTranslation);
	DirectX::XMFLOAT3 move; DirectX::XMStoreFloat3(&move, vWorldTranslation);

	if (usePhysics)
	{
		if (!isGrounded) velocityY -= gravity * elapsedTime;
		move.y = velocityY * elapsedTime;
		MoveAndSlide({ move.x, 0, move.z }, false);
		MoveAndSlide({ 0, move.y, 0 }, true);
	}
	else
	{
		position.x += move.x;
		position.z += move.z;
	}

	GetActor()->SetPosition(position);

	DirectX::XMFLOAT4 rot;
	DirectX::XMStoreFloat4(&rot, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, characterYaw, 0.0f));
	GetActor()->SetRotation(rot);

	// =======================================================
	// 4. カメラの更新（GUIで操作できるように修正）
	// =======================================================
	// ★プロの修正：足元から「cameraOffsetPos」分だけカメラを持ち上げる
	DirectX::XMVECTOR vEyePos = DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&position), DirectX::XMLoadFloat3(&cameraOffsetPos));
	DirectX::XMMATRIX R_Camera = DirectX::XMMatrixRotationRollPitchYaw(cameraPitch, characterYaw, 0.0f);
	DirectX::XMVECTOR vTargetDirection = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), R_Camera);
	DirectX::XMVECTOR vTargetPos = DirectX::XMVectorAdd(vEyePos, vTargetDirection);

	DirectX::XMFLOAT3 eyeF, targetF;
	DirectX::XMStoreFloat3(&eyeF, vEyePos);
	DirectX::XMStoreFloat3(&targetF, vTargetPos);
	camera->SetLookAt(eyeF, targetF, { 0.0f, 1.0f, 0.0f });

	float screenW = Graphics::Instance().GetScreenWidth();
	float screenH = Graphics::Instance().GetScreenHeight();
	camera->SetPerspectiveFov(DirectX::XMConvertToRadians(60), screenW / screenH, 0.35f, 1000.0f);

	// =======================================================
	// 5. ボーンアタッチメント（GUIのボーン名を使用）
	// =======================================================
	if (weaponActor)
	{
		// ★プロの修正：GUIから入力されたボーン名(handBoneName)を使って検索する
		int handBoneIndex = characterModel->GetNodeIndex(handBoneName);
		if (handBoneIndex >= 0)
		{
			const Model::Node& handNode = characterModel->GetNodes().at(handBoneIndex);
			DirectX::XMMATRIX mHandWorld = DirectX::XMLoadFloat4x4(&handNode.worldTransform);
			DirectX::XMMATRIX mPlayerWorld = DirectX::XMMatrixRotationRollPitchYaw(0.0f, characterYaw, 0.0f) *
				DirectX::XMMatrixTranslation(position.x, position.y, position.z);

			DirectX::XMMATRIX mFinalWeaponWorld = mHandWorld * mPlayerWorld;
			DirectX::XMMATRIX mOffset = DirectX::XMMatrixRotationRollPitchYaw(weaponOffsetRot.x, weaponOffsetRot.y, weaponOffsetRot.z) *
				DirectX::XMMatrixTranslation(weaponOffsetPos.x, weaponOffsetPos.y, weaponOffsetPos.z);
			DirectX::XMMATRIX mCameraPitch = DirectX::XMMatrixRotationRollPitchYaw(cameraPitch, 0.0f, 0.0f);

			mFinalWeaponWorld = mOffset * mCameraPitch * mFinalWeaponWorld;

			// ★プロのハック：スケール値を「武器オフセット」から取得するように変更
			// ※現在、weaponOffsetPos/Rotしかないので、スケール用変数も作ります
			DirectX::XMMATRIX mScale = DirectX::XMMatrixScaling(weaponOffsetScale.x, weaponOffsetScale.y, weaponOffsetScale.z);

			// 行列計算にスケールを含める
			mFinalWeaponWorld = mScale * mFinalWeaponWorld;

			DirectX::XMVECTOR scaleVec, rotQuatVec, transVec;
			DirectX::XMMatrixDecompose(&scaleVec, &rotQuatVec, &transVec, mFinalWeaponWorld);

			DirectX::XMFLOAT3 wPos; DirectX::XMStoreFloat3(&wPos, transVec);
			DirectX::XMFLOAT4 wRot; DirectX::XMStoreFloat4(&wRot, rotQuatVec);
			DirectX::XMFLOAT3 wScale; DirectX::XMStoreFloat3(&wScale, scaleVec);

			weaponActor->SetPosition(wPos);
			weaponActor->SetRotation(wRot);
			weaponActor->SetScale(wScale);
		}
	}
}

void PlayerControllerComponent::OnGUI()
{
	ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), u8"▼ FPS設定・操作");
	ImGui::Text(u8"【TABキー】を押してマウス操作 / ImGui操作 を切り替え");
	ImGui::Checkbox(u8"マウスロック(FPS操作中)", &isCursorLocked);

	// ★追加：カメラの位置をリアルタイムでいじれるように
	ImGui::DragFloat3(u8"カメラ 位置オフセット", &cameraOffsetPos.x, 0.01f, -5.0f, 5.0f);

	ImGui::DragFloat(u8"移動速度", &manualMoveSpeed, 0.1f, 0.0f, 20.0f);
	ImGui::DragFloat(u8"ジャンプ力", &jumpPower, 0.1f, 0.0f, 20.0f);
	ImGui::DragFloat(u8"マウス感度", &mouseSensitivity, 0.0001f, 0.0f, 0.01f, "%.4f");

	ImGui::Separator();
	ImGui::TextColored(ImVec4(1, 1, 0, 1), u8"▼ FPS武器アタッチメント設定");

	// ★追加：ボーン名が間違っていると描画されないため、GUIで打ち込めるように
	ImGui::InputText(u8"右手ボーン名", handBoneName, sizeof(handBoneName));
	// OnGUIの中にこれを追加
	
	ImGui::DragFloat3(u8"武器 位置オフセット", &weaponOffsetPos.x, 0.01f, -2.0f, 2.0f);
	ImGui::DragFloat3(u8"武器 回転オフセット", &weaponOffsetRot.x, 0.01f, -DirectX::XM_PI, DirectX::XM_PI);

	ImGui::Separator();
	ImGui::TextColored(ImVec4(0, 1, 1, 1), u8"▼ 内部状態モニター");
	ImGui::Text(u8"カメラ Pitch (上下): %.2f", cameraPitch);
	ImGui::Text(u8"プレイヤー Yaw (左右): %.2f", characterYaw);
}

// 物理演算部（変更なし）
bool PlayerControllerComponent::SphereCast(const DirectX::XMFLOAT3& origin, const DirectX::XMFLOAT3& direction, float radius, float& distance, DirectX::XMFLOAT3& hitPosition, DirectX::XMFLOAT3& hitNormal)
{
	bool hit = false;
	DirectX::XMVECTOR Start = DirectX::XMLoadFloat3(&origin);
	DirectX::XMVECTOR Direction = DirectX::XMLoadFloat3(&direction);
	DirectX::XMVECTOR End = DirectX::XMVectorAdd(Start, DirectX::XMVectorScale(Direction, distance));
	for (const CollisionMesh::Triangle& triangle : collisionMesh.triangles) {
		DirectX::XMVECTOR Positions[3] = { DirectX::XMLoadFloat3(&triangle.positions[0]), DirectX::XMLoadFloat3(&triangle.positions[1]), DirectX::XMLoadFloat3(&triangle.positions[2]) };
		SphereCastResult result;
		if (IntersectSphereCastVsTriangle(Start, End, radius, Positions, &result)) {
			if (distance < result.distance) continue;
			distance = result.distance;
			DirectX::XMVECTOR HitPosition = DirectX::XMVectorAdd(Start, DirectX::XMVectorScale(Direction, distance));
			DirectX::XMStoreFloat3(&hitPosition, HitPosition);
			DirectX::XMStoreFloat3(&hitNormal, result.normal);
			hit = true;
		}
	}
	return hit;
}

void PlayerControllerComponent::MoveAndSlide(const DirectX::XMFLOAT3& move, bool vertical)
{
	DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&position);
	DirectX::XMVECTOR Move = DirectX::XMLoadFloat3(&move);
	DirectX::XMVECTOR CenterOffset = DirectX::XMVectorSet(0.0f, radius + stepOffset, 0.0f, 0.0f);
	for (int i = 0; i < 3; ++i) {
		float moveDistance = DirectX::XMVectorGetX(DirectX::XMVector3Length(Move));
		if (moveDistance < 0.0001f) break;
		float castDistance = moveDistance + skinWidth;
		if (vertical) castDistance += stepOffset;
		DirectX::XMVECTOR Origin = DirectX::XMVectorAdd(Position, CenterOffset);
		DirectX::XMVECTOR Direction = DirectX::XMVector3Normalize(Move);
		DirectX::XMFLOAT3 origin, direction, hitPosition, hitNormal;
		DirectX::XMStoreFloat3(&origin, Origin); DirectX::XMStoreFloat3(&direction, Direction);
		if (SphereCast(origin, direction, radius, castDistance, hitPosition, hitNormal)) {
			castDistance -= skinWidth; if (vertical) castDistance -= stepOffset;
			DirectX::XMVECTOR Vec = DirectX::XMVectorScale(Direction, castDistance);
			Position = DirectX::XMVectorAdd(Position, Vec);
			DirectX::XMVECTOR HitNormal = DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&hitNormal));
			DirectX::XMVECTOR Up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			float dot = DirectX::XMVectorGetX(DirectX::XMVector3Dot(HitNormal, Up));
			dot = std::clamp(dot, -1.0f, 1.0f);
			if (DirectX::XMConvertToDegrees(acosf(dot)) <= slopeLimit) break;
			Move = DirectX::XMVectorSubtract(Move, Vec);
			DirectX::XMVECTOR dotVec = DirectX::XMVector3Dot(Move, HitNormal);
			if (DirectX::XMVectorGetX(dotVec) < 0.0f) Move = DirectX::XMVectorSubtract(Move, DirectX::XMVectorMultiply(HitNormal, dotVec));
			if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(Move)) < 0.00001f) break;
		}
		else {
			Position = DirectX::XMVectorAdd(Position, Move);
			break;
		}
	}
	DirectX::XMStoreFloat3(&position, Position);
}