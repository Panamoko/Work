#pragma once
#include <DirectXMath.h>
#include "GameObject/Component.h"
#include "GameObject/Weapon/Weapon.h"

class Camera;
class PlayerControllerComponent;
class ModelComponent;

// ─────────────────────────────────────────────────────────────
//  WeaponComponent : FPS武器の手続き的モーション制御
//  仕様⑥ オフセットスタック順：
//    Socket → SocketOffset → ADS → Recoil → Sway → Bob → Breath → Final
//  ・武器Actorに付ける（ModelComponent=武器glb / Animator を保持）
//  ・Player本体の全身ModelComponentを「ソケット元(Hand_R)」として参照
//  ・Playerは Update()/Render() を呼ぶだけ（本体は独立して完結）
// ─────────────────────────────────────────────────────────────
class WeaponComponent : public Component
{
public:
	WeaponComponent(Camera* camera, PlayerControllerComponent* playerCtrl);
	~WeaponComponent() override = default;

	const char* GetName() const override { return "WeaponComponent"; }

	void Start()  override;
	void Update(float elapsedTime) override;   // = 仕様の Update()
	void Draw()   override;                     // = 仕様の Render()
	void OnGUI()  override;

	void Render() { Draw(); }                   // Player向けの明示エイリアス

	void Fire();                                // ⑨ リコイル・インパルス
	void SetADS(bool on) { adsTarget = on ? 1.0f : 0.0f; }
	const Weapon& GetWeapon() const { return weapon; }

	void SaveSettings();
	void LoadSettings();

private:
	DirectX::XMMATRIX BuildSocketMatrix();      // ① Hand_R ワールド（or カメラ相対）
	DirectX::XMMATRIX BuildSeatMatrix() const;  // SocketOffset ＋ ⑦ADSラープ
	void UpdateADS(float dt);
	void UpdateSway(float dt);
	void UpdateRecoil(float dt);
	void UpdateBob(float dt);
	void UpdateBreath(float dt);

	Camera* camera = nullptr;
	PlayerControllerComponent* playerController = nullptr;

	Weapon          weapon;                     // この武器Actor自身のモデル
	ModelComponent* bodyModel = nullptr;        // Player全身モデル（ソケット元）
	char socketBoneName[64] = "Hand_R";         // 見つからなければ hand/wrist/arm 検索

	// ── SocketOffset（ヒップ姿勢）と ADS姿勢 ──
	DirectX::XMFLOAT3 hipOffsetPos = { 0.15f, -0.15f, 0.30f };
	DirectX::XMFLOAT3 hipOffsetRot = { 0.0f,  0.0f,  0.0f };
	DirectX::XMFLOAT3 adsOffsetPos = { 0.0f, -0.05f, 0.22f };
	DirectX::XMFLOAT3 adsOffsetRot = { 0.0f,  0.0f,  0.0f };
	DirectX::XMFLOAT3 offsetScale = { 1.0f,  1.0f,  1.0f };

	// ⑦ ADS
	float adsTarget = 0.0f;   // 0=腰だめ 1=覗き込み
	float ads = 0.0f;   // 平滑化後
	float adsSpeed = 12.0f;

	// ⑧ Sway（視点移動の遅れ）
	DirectX::XMFLOAT3 prevFront = { 0, 0, 1 };
	float swayYaw = 0.0f, swayPitch = 0.0f;
	float swayAmount = 1.2f;   // 視点デルタ→スウェイ量
	float swayReturn = 6.0f;   // 中心復帰速度
	float swayMax = 0.06f;  // クランプ(rad)

	// ⑨ Recoil（武器のみ・後退/跳ね上げ/横ブレ、減衰）
	DirectX::XMFLOAT3 recoilPos = { 0, 0, 0 };
	DirectX::XMFLOAT3 recoilRot = { 0, 0, 0 };
	float recoilKickBack = 0.06f;
	float recoilKickUp = 0.05f;
	float recoilKickSide = 0.015f;
	float recoilReturn = 9.0f;
	unsigned int recoilSeed = 2463534242u;

	// ⑩ Bob（歩行 sin/cos）
	float bobPhase = 0.0f;
	float bobAmount = 0.025f;
	float bobSpeed = 9.0f;
	DirectX::XMFLOAT3 bobOffset = { 0, 0, 0 };

	// ⑪ Breath（待機の微揺れ）
	float breathPhase = 0.0f;
	float breathAmount = 0.010f;
	float breathSpeed = 1.6f;
	DirectX::XMFLOAT3 breathOffset = { 0, 0, 0 };

	// カメラ相対補正（TP腕位置問題の対策スイッチ）
	bool  cameraRelative = false;   // true=カメラ前のクラシックなビューモデル

	bool showGrip = false;          // グリップ点デバッグ表示
};