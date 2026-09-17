#include <memory>
#include <sstream>
#include <imgui.h>

#include "Framework.h"
#include "Graphics/Graphics.h"
#include "UI/ImGuiRenderer.h"
#include "Scene/SceneGame.h"
#include "Core/Input.h" // ★ 入力システムをインクルード

#include<tracy/Tracy.hpp>

// 垂直同期間隔設定
static const int syncInterval = 1;

// コンストラクタ
Framework::Framework(HWND hWnd)
	: hWnd(hWnd)
{
	// グラフィックス初期化
	Graphics::Instance().Initialize(hWnd);

	// IMGUI初期化
	ImGuiRenderer::Initialize(hWnd, Graphics::Instance().GetDevice(), Graphics::Instance().GetDeviceContext());

	// ★ 入力システムの初期化（必ずSceneの生成より前に行う）
	Input::Instance().Initialize(hWnd);

	// シーン初期化（コンポーネント指向テスト用）
	scene = std::make_unique<SceneGame>();
}

// デストラクタ
Framework::~Framework()
{
	// IMGUI終了化
	ImGuiRenderer::Finalize();
}

// 更新処理
void Framework::Update(float elapsedTime)
{
	// ★ 最優先処理：ゲームロジックが動く前に、全入力デバイスの状態を最新にする！
	Input::Instance().Update();

	ImGuiRenderer::NewFrame();
	scene->Update(elapsedTime);
}

// 描画処理
void Framework::Render(float elapsedTime)
{
	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();

	// Render the scene into an offscreen HDR target.
	Graphics::Instance().BeginScene(0.5f, 0.5f, 0.5f, 1.0f);
	scene->Render(elapsedTime);

	// Post-process: offscreen -> back buffer (passthrough for now).
	Graphics::Instance().ApplyPostProcess();

	// GUI on the back buffer.
	scene->DrawGUI();
	ImGuiRenderer::Render(dc);

	// Present.
	Graphics::Instance().Present(syncInterval);
}

// テンプレート関数とGUI関数の空定義（ヘッダーとの整合性維持）
template<class T>
void Framework::ChangeSceneButtonGUI(const char* name) {}

void Framework::SceneSelectGUI() {}

// フレームレート計算
void Framework::CalculateFrameStats()
{
	static int frames = 0;
	static float time_tlapsed = 0.0f;
	frames++;

	if ((timer.TimeStamp() - time_tlapsed) >= 1.0f)
	{
		float fps = static_cast<float>(frames);
		float mspf = 1000.0f / fps;
		std::ostringstream outs;
		outs.precision(6);
		outs << "FPS : " << fps << " / " << "Frame Time : " << mspf << " (ms)";
		SetWindowTextA(hWnd, outs.str().c_str());

		frames = 0;
		time_tlapsed += 1.0f;
	}
}

// アプリケーションループ
int Framework::Run()
{
	MSG msg = {};

	while (WM_QUIT != msg.message)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			timer.Tick();
			CalculateFrameStats();

			float elapsedTime = timer.TimeInterval();
			Update(elapsedTime);
			Render(elapsedTime);
		}
	}
	return static_cast<int>(msg.wParam);
}

// メッセージハンドラ
LRESULT CALLBACK Framework::HandleMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGuiRenderer::HandleMessage(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc;
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;
	}
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_CREATE:
		break;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) PostMessage(hWnd, WM_CLOSE, 0, 0);
		break;

		// ★ 落とし穴の修正：マウスホイールの回転をOSから受け取り、Inputシステムに流す
	case WM_MOUSEWHEEL:
		// WHEEL_DELTA(120)で割ることで、手前にカリッと回すと -1、奥に回すと 1 が入る
		Input::Instance().GetMouse().SetWheel(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
		break;

	case WM_ENTERSIZEMOVE:
		timer.Stop();
		break;
	case WM_EXITSIZEMOVE:
		timer.Start();
		break;
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED && Graphics::Instance().GetDevice() != nullptr)
		{
			UINT width = LOWORD(lParam);
			UINT height = HIWORD(lParam);

			// グラフィックスエンジンにキャンバスの再生成を命令
			Graphics::Instance().Resize(width, height);

			// マウスの座標系も新しい解像度に合わせて更新
			Input::Instance().GetMouse().SetScreenWidth(width);
			Input::Instance().GetMouse().SetScreenHeight(height);
		}
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}