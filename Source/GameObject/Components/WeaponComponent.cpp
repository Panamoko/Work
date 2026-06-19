#include <imgui.h>
#include <fstream>
#include "GameObject/Components/WeaponComponent.h"
#include "GameObject/Actor.h"
#include "Graphics/Model/Model.h" 

void WeaponComponent::Start()
{
	LoadSettings(); // ゲーム開始時に設定を復元
}

void WeaponComponent::SaveSettings()
{
	std::ofstream ofs("WeaponSettings.txt");
	if (ofs) {
		ofs << targetBoneName << "\n";
		ofs << offsetPos.x << " " << offsetPos.y << " " << offsetPos.z << "\n";
		ofs << offsetRot.x << " " << offsetRot.y << " " << offsetRot.z << "\n";
		ofs << offsetScale.x << " " << offsetScale.y << " " << offsetScale.z << "\n";
	}
}

void WeaponComponent::LoadSettings()
{
	std::ifstream ifs("WeaponSettings.txt");
	if (ifs) {
		ifs >> targetBoneName;
		ifs >> offsetPos.x >> offsetPos.y >> offsetPos.z;
		ifs >> offsetRot.x >> offsetRot.y >> offsetRot.z;
		ifs >> offsetScale.x >> offsetScale.y >> offsetScale.z;
	}
}

void WeaponComponent::SetWeaponTransform(const DirectX::XMMATRIX& parentMatrix)
{
	// ★行列の計算順序：自身のScale -> 自身のOffset -> 親(右手)の空間
	DirectX::XMMATRIX mScale = DirectX::XMMatrixScaling(offsetScale.x, offsetScale.y, offsetScale.z);
	DirectX::XMMATRIX mOffset = DirectX::XMMatrixRotationRollPitchYaw(offsetRot.x, offsetRot.y, offsetRot.z) * DirectX::XMMatrixTranslation(offsetPos.x, offsetPos.y, offsetPos.z);
	DirectX::XMMATRIX mFinal = mScale * mOffset * parentMatrix;

	DirectX::XMVECTOR scaleVec, rotQuatVec, transVec;
	DirectX::XMMatrixDecompose(&scaleVec, &rotQuatVec, &transVec, mFinal);

	DirectX::XMFLOAT3 wPos, wScale; DirectX::XMFLOAT4 wRot;
	DirectX::XMStoreFloat3(&wPos, transVec);
	DirectX::XMStoreFloat4(&wRot, rotQuatVec);
	DirectX::XMStoreFloat3(&wScale, scaleVec);

	GetActor()->SetPosition(wPos);
	GetActor()->SetRotation(wRot);
	GetActor()->SetScale(wScale);
}

void WeaponComponent::OnGUI()
{
	ImGui::TextColored(ImVec4(1, 1, 0, 1), u8"▼ 武器アタッチメント設定 (WeaponComponent)");
	if (ImGui::Button(u8"【 武器の設定をファイルに保存する 】")) SaveSettings();
	ImGui::Separator();

	ImGui::InputText(u8"追従するボーン名", targetBoneName, sizeof(targetBoneName));

	// ★プロの完全復元：緑色/赤色のステータス表示
	if (currentConnectedBoneIndex >= 0) {
		ImGui::TextColored(ImVec4(0, 1, 0, 1), u8"ステータス: 接続成功 (Index: %d)", currentConnectedBoneIndex);
	}
	else {
		ImGui::TextColored(ImVec4(1, 0, 0, 1), u8"ステータス: エラー！ボーンが見つかりません");
	}

	// Playerのボーンリスト展開
	if (ImGui::TreeNode(u8"Playerの全ボーン名一覧を見る（クリックで自動入力）")) {
		if (targetModel) {
			const auto& nodes = targetModel->GetNodes();
			for (size_t i = 0; i < nodes.size(); ++i) {
				if (ImGui::Selectable(nodes[i].name.c_str())) SetTargetBoneName(nodes[i].name.c_str());
			}
		}
		else {
			ImGui::TextColored(ImVec4(1, 0, 0, 1), u8"Playerモデルがリンクされていません");
		}
		ImGui::TreePop();
	}

	ImGui::DragFloat3(u8"武器 位置オフセット", &offsetPos.x, 0.1f);
	ImGui::DragFloat3(u8"武器 回転オフセット", &offsetRot.x, 0.01f, -DirectX::XM_PI, DirectX::XM_PI);
	ImGui::DragFloat3(u8"武器 スケール(X/Y/Z)", &offsetScale.x, 0.1f, 0.01f, 100.0f);
}