#include "GameObject/Components/WeaponComponent.h"
#include "GameObject/Actor.h"
#include "GameObject/Components/ModelComponent.h"
#include "GameObject/Components/AnimatorComponent.h"
#include "GameObject/Components/PlayerControllerComponent.h"
#include "Graphics/Model/Model.h"
#include "Graphics/Graphics.h"
#include "Graphics/Camera.h"
#include "Core/Input.h"
#include <imgui.h>
#include <fstream>
#include <string>
#include <cctype>
#include <cmath>
#include <algorithm>

using namespace DirectX;

// ---- ローカルヘルパ -------------------------------------------------------
static float ApproachLerp(float cur, float tgt, float rate, float dt)
{
	float k = rate * dt;
	if (k > 1.0f) k = 1.0f;
	return cur + (tgt - cur) * k;
}

static float Clampf(float v, float lo, float hi)
{
	return (v < lo) ? lo : (v > hi) ? hi : v;
}

// Hand_R が名前で見つからない時のフォールバック探索（ViewModelと同じ規約）
static int FindHandNode(Model* m)
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

// 軽量ホワイトノイズ（リコイルの横ブレ用、-1..1）
static float NextNoise(unsigned int& s)
{
	s ^= s << 13; s ^= s >> 17; s ^= s << 5;
	return (static_cast<float>(s & 0xFFFF) / 32768.0f) - 1.0f;
}

// ---- ライフサイクル -------------------------------------------------------
WeaponComponent::WeaponComponent(Camera* camera, PlayerControllerComponent* playerCtrl)
	: camera(camera), playerController(playerCtrl)
{
}

void WeaponComponent::Start()
{
	weapon.Bind(
		GetActor()->GetComponent<ModelComponent>().get(),
		GetActor()->GetComponent<AnimatorComponent>().get());

	if (playerController) bodyModel = playerController->GetModelComponent();
	if (camera) prevFront = camera->GetFront();

	LoadSettings();
	weapon.PlayAnim("Pistol_IDLE", 0.1f, true);   // あれば待機再生
}

// ---- ⑥ オフセットスタック合成 -------------------------------------------
void WeaponComponent::Update(float elapsedTime)
{
	if (!camera || !weapon.IsValid()) return;

	// 入力：右クリックでADS、左クリックで発砲
	Mouse& mouse = Input::Instance().GetMouse();
	SetADS((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
	if (mouse.GetButtonDown() & Mouse::BTN_LEFT) Fire();

	// 各オフセットの状態更新
	UpdateADS(elapsedTime);
	UpdateSway(elapsedTime);
	UpdateRecoil(elapsedTime);
	UpdateBob(elapsedTime);
	UpdateBreath(elapsedTime);

	// 行列を組み立て（DirectXMathは行ベクトル：A*B は「Aを先に適用」）
	XMMATRIX mSocket = BuildSocketMatrix();                                  // ① 手のワールド
	XMMATRIX mSeat = BuildSeatMatrix();                                    // SocketOffset(＋ADS)

	XMMATRIX mRecoil = XMMatrixRotationRollPitchYaw(recoilRot.x, recoilRot.y, recoilRot.z)
		* XMMatrixTranslation(recoilPos.x, recoilPos.y, recoilPos.z);
	XMMATRIX mSway = XMMatrixRotationRollPitchYaw(swayPitch, swayYaw, 0.0f);
	XMMATRIX mBob = XMMatrixTranslation(bobOffset.x, bobOffset.y, bobOffset.z);
	XMMATRIX mBreath = XMMatrixTranslation(breathOffset.x, breathOffset.y, breathOffset.z);
	XMMATRIX mScale = XMMatrixScaling(offsetScale.x, offsetScale.y, offsetScale.z);

	// Socketに近い側から: Seat → Recoil → Sway → Bob → Breath（Breathが最外周）
	XMMATRIX mLocal = mScale * mBreath * mBob * mSway * mRecoil * mSeat;
	XMMATRIX mFinal = mLocal * mSocket;

	XMFLOAT4X4 world;
	XMStoreFloat4x4(&world, mFinal);
	weapon.SetWorldTransform(world);
}

DirectX::XMMATRIX WeaponComponent::BuildSocketMatrix()
{
	// カメラ相対補正：TP共有モデルだと Hand_R はTPアニメに従うので、武器を
	// 剛体で載せると“体の手の位置”に来てしまう（カメラ前に構えない）。
	// cameraRelative=true でカメラ座標系を基準にした古典的ビューモデルにする。
	if (cameraRelative || !bodyModel)
	{
		XMVECTOR R = XMLoadFloat3(&camera->GetRight());
		XMVECTOR U = XMLoadFloat3(&camera->GetUp());
		XMVECTOR F = XMLoadFloat3(&camera->GetFront());
		XMVECTOR E = XMLoadFloat3(&camera->GetEye());
		XMMATRIX m;
		m.r[0] = XMVectorSetW(R, 0.0f);
		m.r[1] = XMVectorSetW(U, 0.0f);
		m.r[2] = XMVectorSetW(F, 0.0f);
		m.r[3] = XMVectorSetW(E, 1.0f);
		return m;
	}

	// Player全身モデルのインスタンス毎ポーズから Hand_R のワールドを取得
	const std::vector<Model::Node>& nodes = bodyModel->GetPoseNodes();
	if (!nodes.empty())
	{
		int ni = -1;
		if (socketBoneName[0] != '\0' && bodyModel->GetModel())
			ni = bodyModel->GetModel()->GetNodeIndex(socketBoneName);
		if (ni < 0 && bodyModel->GetModel())
			ni = FindHandNode(bodyModel->GetModel().get());
		if (ni < 0 || ni >= static_cast<int>(nodes.size())) ni = 0;
		return XMLoadFloat4x4(&nodes[ni].worldTransform);
	}
	return XMMatrixIdentity();
}

DirectX::XMMATRIX WeaponComponent::BuildSeatMatrix() const
{
	// ⑦ ADS：ヒップ⇔覗き込み姿勢を smoothstep で補間
	float t = ads * ads * (3.0f - 2.0f * ads);
	XMFLOAT3 p, r;
	p.x = hipOffsetPos.x + (adsOffsetPos.x - hipOffsetPos.x) * t;
	p.y = hipOffsetPos.y + (adsOffsetPos.y - hipOffsetPos.y) * t;
	p.z = hipOffsetPos.z + (adsOffsetPos.z - hipOffsetPos.z) * t;
	r.x = hipOffsetRot.x + (adsOffsetRot.x - hipOffsetRot.x) * t;
	r.y = hipOffsetRot.y + (adsOffsetRot.y - hipOffsetRot.y) * t;
	r.z = hipOffsetRot.z + (adsOffsetRot.z - hipOffsetRot.z) * t;

	return XMMatrixRotationRollPitchYaw(r.x, r.y, r.z)
		* XMMatrixTranslation(p.x, p.y, p.z);
}

// ---- 各オフセットの更新 ---------------------------------------------------
void WeaponComponent::UpdateADS(float dt)
{
	ads = ApproachLerp(ads, adsTarget, adsSpeed, dt);
}

void WeaponComponent::UpdateSway(float dt)
{
	// 視点の毎フレーム差分をインパルスとして加え、常に中心へ復帰させる遅れ表現
	XMFLOAT3 f = camera->GetFront();
	float dYaw = std::atan2(f.x, f.z) - std::atan2(prevFront.x, prevFront.z);
	if (dYaw > XM_PI) dYaw -= XM_2PI;
	if (dYaw < -XM_PI) dYaw += XM_2PI;
	float dPitch = std::asin(Clampf(-f.y, -1.0f, 1.0f)) - std::asin(Clampf(-prevFront.y, -1.0f, 1.0f));
	prevFront = f;

	float scale = swayAmount * (1.0f - 0.7f * ads);   // ADS中はブレを抑制
	swayYaw = ApproachLerp(swayYaw, 0.0f, swayReturn, dt) + (-dYaw * scale);
	swayPitch = ApproachLerp(swayPitch, 0.0f, swayReturn, dt) + (dPitch * scale);
	swayYaw = Clampf(swayYaw, -swayMax, swayMax);
	swayPitch = Clampf(swayPitch, -swayMax, swayMax);
}

void WeaponComponent::UpdateRecoil(float dt)
{
	// 減衰（発砲時に Fire() がインパルスを積む）
	recoilPos.x = ApproachLerp(recoilPos.x, 0.0f, recoilReturn, dt);
	recoilPos.y = ApproachLerp(recoilPos.y, 0.0f, recoilReturn, dt);
	recoilPos.z = ApproachLerp(recoilPos.z, 0.0f, recoilReturn, dt);
	recoilRot.x = ApproachLerp(recoilRot.x, 0.0f, recoilReturn, dt);
	recoilRot.y = ApproachLerp(recoilRot.y, 0.0f, recoilReturn, dt);
	recoilRot.z = ApproachLerp(recoilRot.z, 0.0f, recoilReturn, dt);
}

void WeaponComponent::UpdateBob(float dt)
{
	bool moving = false;
	bool running = false;
	if (playerController)
	{
		PlayerState s = playerController->GetCurrentState();
		moving = (s == PlayerState::Walk || s == PlayerState::Run);
		running = (s == PlayerState::Run);
	}

	if (moving)
	{
		float spd = bobSpeed * (running ? 1.4f : 1.0f);
		float amp = bobAmount * (running ? 1.5f : 1.0f) * (1.0f - 0.8f * ads);
		bobPhase += dt * spd;
		float tx = std::cos(bobPhase) * amp;
		float ty = std::sin(bobPhase * 2.0f) * amp * 0.6f;   // 8の字：縦は2倍周期
		bobOffset.x = ApproachLerp(bobOffset.x, tx, 12.0f, dt);
		bobOffset.y = ApproachLerp(bobOffset.y, ty, 12.0f, dt);
	}
	else
	{
		bobOffset.x = ApproachLerp(bobOffset.x, 0.0f, 8.0f, dt);
		bobOffset.y = ApproachLerp(bobOffset.y, 0.0f, 8.0f, dt);
	}
}

void WeaponComponent::UpdateBreath(float dt)
{
	breathPhase += dt * breathSpeed;
	float amp = breathAmount * (1.0f - 0.9f * ads);   // ADS中は息を止める
	breathOffset.x = std::sin(breathPhase) * amp;
	breathOffset.y = std::cos(breathPhase * 0.7f) * amp;
}

void WeaponComponent::Fire()
{
	// ⑨ 武器のみに後退／跳ね上げ／横ブレを与える（本体・カメラは触らない）
	recoilPos.z -= recoilKickBack;
	recoilPos.y += recoilKickUp * 0.3f;
	recoilRot.x -= recoilKickUp;                       // ピッチ跳ね上げ
	recoilRot.z += NextNoise(recoilSeed) * recoilKickSide; // ロール横ブレ
	weapon.PlayAnim("Pistol_FIRE", 0.03f, false);
}

// ---- 描画（= Render）：グリップ点のデバッグ表示のみ ----------------------
void WeaponComponent::Draw()
{
	if (!showGrip || !weapon.IsValid()) return;
	XMFLOAT4X4 grip = weapon.GetGripPoint();
	XMFLOAT3 p = { grip._41, grip._42, grip._43 };
	Graphics::Instance().GetShapeRenderer()->DrawSphere(p, 0.02f, { 0.2f, 1.0f, 0.3f, 1.0f });
}

// ---- 設定の保存／読み込み -------------------------------------------------
void WeaponComponent::SaveSettings()
{
	std::ofstream ofs("WeaponSettings.txt");
	if (!ofs) return;
	ofs << hipOffsetPos.x << " " << hipOffsetPos.y << " " << hipOffsetPos.z << "\n";
	ofs << hipOffsetRot.x << " " << hipOffsetRot.y << " " << hipOffsetRot.z << "\n";
	ofs << adsOffsetPos.x << " " << adsOffsetPos.y << " " << adsOffsetPos.z << "\n";
	ofs << adsOffsetRot.x << " " << adsOffsetRot.y << " " << adsOffsetRot.z << "\n";
	ofs << offsetScale.x << " " << offsetScale.y << " " << offsetScale.z << "\n";
	ofs << recoilKickBack << " " << recoilKickUp << " " << recoilKickSide << "\n";
	ofs << swayAmount << " " << swayMax << " " << bobAmount << " " << breathAmount << "\n";
	ofs << (cameraRelative ? 1 : 0) << "\n";
	ofs << (socketBoneName[0] ? socketBoneName : "Hand_R") << "\n";
}

void WeaponComponent::LoadSettings()
{
	std::ifstream ifs("WeaponSettings.txt");
	if (!ifs) return;
	ifs >> hipOffsetPos.x >> hipOffsetPos.y >> hipOffsetPos.z;
	ifs >> hipOffsetRot.x >> hipOffsetRot.y >> hipOffsetRot.z;
	ifs >> adsOffsetPos.x >> adsOffsetPos.y >> adsOffsetPos.z;
	ifs >> adsOffsetRot.x >> adsOffsetRot.y >> adsOffsetRot.z;
	ifs >> offsetScale.x >> offsetScale.y >> offsetScale.z;
	ifs >> recoilKickBack >> recoilKickUp >> recoilKickSide;
	ifs >> swayAmount >> swayMax >> bobAmount >> breathAmount;
	int cr = 0; ifs >> cr; cameraRelative = (cr != 0);
	std::string bone;
	if (ifs >> bone && bone != "none") strncpy_s(socketBoneName, bone.c_str(), _TRUNCATE);
}

// ---- インスペクタ ---------------------------------------------------------
void WeaponComponent::OnGUI()
{
	ImGui::TextColored(ImVec4(1, 0.6f, 0.2f, 1), u8"▼ 武器オフセットスタック");
	ImGui::Text(u8"ADS: %.2f   Recoilback: %.3f", ads, recoilPos.z);
	ImGui::Checkbox(u8"カメラ相対（TP腕対策/ビューモデル化）", &cameraRelative);
	ImGui::InputText(u8"ソケットボーン", socketBoneName, sizeof(socketBoneName));

	ImGui::SeparatorText("SocketOffset / ADS");
	ImGui::DragFloat3(u8"Hip位置", &hipOffsetPos.x, 0.005f);
	ImGui::DragFloat3(u8"Hip回転", &hipOffsetRot.x, 0.01f, -DirectX::XM_PI, DirectX::XM_PI);
	ImGui::DragFloat3(u8"ADS位置", &adsOffsetPos.x, 0.005f);
	ImGui::DragFloat3(u8"ADS回転", &adsOffsetRot.x, 0.01f, -DirectX::XM_PI, DirectX::XM_PI);
	ImGui::DragFloat3(u8"スケール", &offsetScale.x, 0.01f, 0.0001f, 100.0f, "%.4f");

	ImGui::SeparatorText("Recoil / Sway / Bob / Breath");
	ImGui::DragFloat(u8"リコイル後退", &recoilKickBack, 0.005f);
	ImGui::DragFloat(u8"リコイル跳上", &recoilKickUp, 0.005f);
	ImGui::DragFloat(u8"リコイル横ブレ", &recoilKickSide, 0.005f);
	ImGui::DragFloat(u8"スウェイ量", &swayAmount, 0.05f);
	ImGui::DragFloat(u8"スウェイ上限", &swayMax, 0.005f);
	ImGui::DragFloat(u8"ボブ量", &bobAmount, 0.002f);
	ImGui::DragFloat(u8"ブレス量", &breathAmount, 0.001f);

	ImGui::Checkbox(u8"グリップ点表示", &showGrip);
	if (ImGui::Button(u8"【 武器設定を保存 】")) SaveSettings();
}