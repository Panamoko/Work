#include <algorithm>
#include <imgui.h>
#include "PlayerControllerComponent.h"
#include "Physics/SphereCast.h"
#include "Graphics/Graphics.h"

PlayerControllerComponent::PlayerControllerComponent(Camera* camera, FpsCameraController* cameraController, const CollisionMesh& collisionMesh)
	: camera(camera), cameraController(cameraController), collisionMesh(collisionMesh)
{
}

void PlayerControllerComponent::Start()
{
	// アタッチされた瞬間に、モデルコンポーネントから初期アニメーションを取得
	auto modelComp = GetActor()->GetComponent<ModelComponent>();
	if (modelComp && modelComp->GetModel())
	{
		modelComp->GetModel()->GetNodePoses(nodePoses);
		animationIndex = modelComp->GetModel()->GetAnimationIndex(animIdle);
	}
}

void PlayerControllerComponent::Update(float elapsedTime)
{
	auto modelComp = GetActor()->GetComponent<ModelComponent>();
	if (!modelComp) return;

	Model* characterModel = modelComp->GetModel().get();
	if (!characterModel) return;

	int newAnimationIndex = animationIndex;
	DirectX::XMFLOAT3 manualVec = { 0, 0, 0 };

	// WASD入力
	if (GetAsyncKeyState('W') & 0x8000) { newAnimationIndex = characterModel->GetAnimationIndex(animWalkF); manualVec.z += 1.0f; }
	else if (GetAsyncKeyState('S') & 0x8000) { newAnimationIndex = characterModel->GetAnimationIndex(animWalkB); manualVec.z -= 1.0f; }
	else if (GetAsyncKeyState('D') & 0x8000) { newAnimationIndex = characterModel->GetAnimationIndex(animWalkR); manualVec.x += 1.0f; }
	else if (GetAsyncKeyState('A') & 0x8000) { newAnimationIndex = characterModel->GetAnimationIndex(animWalkL); manualVec.x -= 1.0f; }
	else { newAnimationIndex = characterModel->GetAnimationIndex(animIdle); }

	debugAnimIndex = newAnimationIndex;

	if (animationIndex != newAnimationIndex)
	{
		animationIndex = newAnimationIndex;
		animationSeconds = 0.0f;
		oldAnimationSeconds = 0.0f;
	}

	DirectX::XMVECTOR vRootMotionTranslation = DirectX::XMVectorZero();

	// アニメーション更新とルートモーション抽出
	if (animationIndex >= 0)
	{
		const Model::Animation& animation = characterModel->GetAnimations().at(animationIndex);
		characterModel->ComputeAnimation(animationIndex, animationSeconds, nodePoses);

		const int rootMotionNodeIndex = characterModel->GetNodeIndex(rootNodeName);

		if (rootMotionNodeIndex >= 0)
		{
			std::vector<Model::NodePose> beginPoses = nodePoses;
			characterModel->ComputeAnimation(animationIndex, 0.0f, beginPoses);
			Model::NodePose beginPose = beginPoses.at(rootMotionNodeIndex);

			if (useRootMotion)
			{
				std::vector<Model::NodePose> oldPoses = nodePoses;
				characterModel->ComputeAnimation(animationIndex, oldAnimationSeconds, oldPoses);
				Model::NodePose oldPose = oldPoses.at(rootMotionNodeIndex);
				Model::NodePose newPose = nodePoses.at(rootMotionNodeIndex);

				DirectX::XMFLOAT3 localTranslation = { 0, 0, 0 };
				if (oldAnimationSeconds > animationSeconds)
				{
					std::vector<Model::NodePose> endPoses = nodePoses;
					characterModel->ComputeAnimation(animationIndex, animation.secondsLength, endPoses);
					Model::NodePose endPose = endPoses.at(rootMotionNodeIndex);

					std::vector<Model::NodePose> beginPoses = nodePoses;
					characterModel->ComputeAnimation(animationIndex, 0.0f, beginPoses);
					Model::NodePose beginPose = beginPoses.at(rootMotionNodeIndex);

					localTranslation.x = (endPose.position.x - oldPose.position.x) + (newPose.position.x - beginPose.position.x);
					localTranslation.z = (endPose.position.z - oldPose.position.z) + (newPose.position.z - beginPose.position.z);
				}
				else
				{
					localTranslation.x = newPose.position.x - oldPose.position.x;
					localTranslation.y = newPose.position.y - oldPose.position.y;
					localTranslation.z = newPose.position.z - oldPose.position.z;
				}

				Model::Node& rootMotionNode = characterModel->GetNodes().at(rootMotionNodeIndex);
				DirectX::XMVECTOR vLocalTranslation = DirectX::XMLoadFloat3(&localTranslation);
				vRootMotionTranslation = vLocalTranslation;

				if (rootMotionNode.parentIndex >= 0)
				{
					Model::NodePose parentPose = nodePoses.at(rootMotionNode.parentIndex);
					DirectX::XMVECTOR parentRot = DirectX::XMLoadFloat4(&parentPose.rotation);
					vRootMotionTranslation = DirectX::XMVector3Rotate(vLocalTranslation, parentRot);
				}

				vRootMotionTranslation = DirectX::XMVectorScale(vRootMotionTranslation, rootMotionScale);
			}

			nodePoses.at(rootMotionNodeIndex).position.x = beginPose.position.x;
			nodePoses.at(rootMotionNodeIndex).position.z = beginPose.position.z;
		}

		oldAnimationSeconds = animationSeconds;
		animationSeconds += elapsedTime * animSpeed;
		if (animationSeconds > animation.secondsLength) animationSeconds -= animation.secondsLength;
		characterModel->SetNodePoses(nodePoses);
	}

	DirectX::XMVECTOR vManualTranslation = DirectX::XMVectorSet(
		manualVec.x * manualMoveSpeed * elapsedTime, 0.0f,
		manualVec.z * manualMoveSpeed * elapsedTime, 0.0f
	);

	DirectX::XMVECTOR vGlobalTranslation = DirectX::XMVectorAdd(vRootMotionTranslation, vManualTranslation);

	DirectX::XMStoreFloat3(&debugRootMotion, vRootMotionTranslation);
	DirectX::XMStoreFloat3(&debugManualMotion, vManualTranslation);

	const DirectX::XMFLOAT3& front = camera->GetFront();
	float targetYaw = atan2f(front.x, front.z);

	float diff = targetYaw - characterYaw;
	while (diff < -DirectX::XM_PI) diff += DirectX::XM_2PI;
	while (diff > DirectX::XM_PI) diff -= DirectX::XM_2PI;
	characterYaw += diff * (std::min)(10.0f * elapsedTime, 1.0f);

	vGlobalTranslation = DirectX::XMVectorSetY(vGlobalTranslation, 0.0f);
	DirectX::XMMATRIX R = DirectX::XMMatrixRotationY(characterYaw);
	DirectX::XMVECTOR vWorldTranslation = DirectX::XMVector3TransformNormal(vGlobalTranslation, R);

	DirectX::XMFLOAT3 move;
	DirectX::XMStoreFloat3(&move, vWorldTranslation);

	// 物理演算
	if (usePhysics)
	{
		move.y = -3.0f * elapsedTime;
		MoveAndSlide({ move.x, 0, move.z }, false);
		MoveAndSlide({ 0, move.y, 0 }, true);
	}
	else
	{
		position.x += move.x;
		position.z += move.z;
	}

	// 自身の所属するアクターに座標と回転を反映させる
	GetActor()->SetPosition(position);

	DirectX::XMFLOAT4 rot;
	DirectX::XMStoreFloat4(&rot, DirectX::XMQuaternionRotationRollPitchYaw(0.0f, characterYaw, 0.0f));
	GetActor()->SetRotation(rot);

	// カメラ更新
	HWND hWnd = GetActiveWindow();
	float eyeHeight = 0.0f;
	DirectX::XMFLOAT3 cameraTargetPos = position;
	{
		DirectX::XMVECTOR FrontXZ = DirectX::XMVectorSet(front.x, 0.0f, front.z, 0.0f);
		if (DirectX::XMVector3LengthSq(FrontXZ).m128_f32[0] < 0.001f) FrontXZ = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		FrontXZ = DirectX::XMVector3Normalize(FrontXZ);
		DirectX::XMVECTOR Offset = DirectX::XMVectorAdd(DirectX::XMVectorScale(FrontXZ, -3.0f), DirectX::XMVectorSet(0.0f, 1.2f, 0.0f, 0.0f));
		DirectX::XMStoreFloat3(&cameraTargetPos, DirectX::XMVectorAdd(DirectX::XMLoadFloat3(&position), Offset));
	}
	cameraController->Update(hWnd, cameraTargetPos, eyeHeight);
	cameraController->SyncControllerToCamera(*camera);
}

void PlayerControllerComponent::OnGUI()
{
	ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), u8"▼ 移動のブレンド設定");
	ImGui::Checkbox(u8"アニメ由来の移動(ルートモーション)", &useRootMotion);
	ImGui::Checkbox(u8"物理演算(床との衝突)", &usePhysics);

	ImGui::Separator();
	ImGui::DragFloat(u8"Manual Speed", &manualMoveSpeed, 0.1f, 0.0f, 20.0f);

	ImGui::Separator();
	ImGui::TextColored(ImVec4(1, 1, 0, 1), u8"▼ アニメーション名");
	ImGui::InputText(u8"待機(Idle)", animIdle, sizeof(animIdle));
	ImGui::InputText(u8"前進(W)", animWalkF, sizeof(animWalkF));
	ImGui::InputText(u8"後退(S)", animWalkB, sizeof(animWalkB));
	ImGui::InputText(u8"左(A)", animWalkL, sizeof(animWalkL));
	ImGui::InputText(u8"右(D)", animWalkR, sizeof(animWalkR));

	ImGui::InputText(u8"ノード名", rootNodeName, sizeof(rootNodeName));
	ImGui::DragFloat(u8"ルートモーション倍率", &rootMotionScale, 0.1f, -10.0f, 10.0f);
	ImGui::SliderFloat(u8"アニメ再生速度", &animSpeed, 0.1f, 5.0f);

	ImGui::Separator();
	ImGui::TextColored(ImVec4(0, 1, 1, 1), u8"▼ 内部状態モニター");
	ImGui::Text(u8"現在のアニメIndex: %d", debugAnimIndex);
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1), u8"① アニメ由来: (%.4f, %.4f)", debugRootMotion.x, debugRootMotion.z);
	ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1), u8"② WASD由来 : (%.4f, %.4f)", debugManualMotion.x, debugManualMotion.z);
}

// 物理演算部（移植）
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