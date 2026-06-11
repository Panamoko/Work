#include <memory>
#include <sstream>
#include <imgui.h>

#include "Framework.h"
#include "Graphics/Graphics.h"
#include "UI/ImGuiRenderer.h"
#include "Scene/SceneGame.h"

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
	ImGuiRenderer::NewFrame();
	scene->Update(elapsedTime);
}

// 描画処理
void Framework::Render(float elapsedTime)
{
	ID3D11DeviceContext* dc = Graphics::Instance().GetDeviceContext();

	// 画面クリア
	Graphics::Instance().Clear(0.5f, 0.5f, 0.5f, 1.0f);

	// レンダーターゲット設定
	Graphics::Instance().SetRenderTargets();

	// シーン描画処理
	scene->Render(elapsedTime);

	// シーンGUI描画処理
	scene->DrawGUI();

	// IMGUI描画
	ImGuiRenderer::Render(dc);

	// 画面表示
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
	case WM_ENTERSIZEMOVE:
		timer.Stop();
		break;
	case WM_EXITSIZEMOVE:
		timer.Start();
		break;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}