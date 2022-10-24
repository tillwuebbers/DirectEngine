#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <mutex>

#include <d3d12.h>
#include <DirectXMath.h>
#include <dxgidebug.h>

#include "core/EngineCore.h"
#include "core/UI.h"
#include "game/IGame.h"
#include "game/Game.h"
#include "DirectEngine.h"

struct GameThreadData
{
	EngineCore* engine;
};

struct EngineUpdate
{
	bool resize;
	UINT resize_width;
	UINT resize_height;

	bool toggleWindowMode;

	bool reloadShaders;
	bool quit;
};

EngineCore* engineCore;
EngineUpdate updateData{};
std::mutex updateDataMutex;

// Watch development files for quick reload of shaders
DWORD WINAPI FileWatcherThread(LPVOID lpParameter)
{
	HANDLE changeNotificationHandle = FindFirstChangeNotification(L"../../../../DirectEngine/shaders", FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
	if (changeNotificationHandle == INVALID_HANDLE_VALUE)
	{
		OutputDebugString(L"Folder change notification failed");
		return 1;
	}

	while (true)
	{
		switch (WaitForSingleObject(changeNotificationHandle, INFINITE))
		{
		case WAIT_OBJECT_0:
			updateDataMutex.lock();
			updateData.reloadShaders = true;
			updateDataMutex.unlock();

			if (!FindNextChangeNotification(changeNotificationHandle))
			{
				OutputDebugString(L"Folder change notification failed");
				return 1;
			}
			break;
		default:
			OutputDebugString(L"Folder change notification failed");
			return 1;
		}
	}

	return 0;
}

// Render loop
DWORD WINAPI GameRenderThread(LPVOID lpParameter)
{
	GameThreadData* data = static_cast<GameThreadData*>(lpParameter);
	EngineCore& engine = *data->engine;

	bool quit = false;
	while (!quit)
	{
		engine.BeginProfile("Mutex Read", ImColor::HSV(.0f, .5f, .5f));
		updateDataMutex.lock();
		if (updateData.resize)
		{
			updateData.resize = false;
			engine.OnResize(updateData.resize_width, updateData.resize_height);
		}
		if (updateData.reloadShaders)
		{
			updateData.reloadShaders = false;
			engine.OnShaderReload();
		}
		if (updateData.toggleWindowMode)
		{
			updateData.toggleWindowMode = false;
			engine.ToggleWindowMode();
		}
		if (updateData.quit)
		{
			quit = true;
		}
		updateDataMutex.unlock();
		engine.EndProfile("Mutex Read");

		engine.BeginProfile("Waitable", ImColor(.3f, .3f, .3f));
		WaitForSingleObjectEx(engine.m_frameWaitableObject, 1000, true);
		engine.EndProfile("Waitable");
		engine.OnUpdate();
		engine.OnRender();
	}

	engine.OnDestroy();
	engineCore = nullptr;

	return 0;
}

// Window event callback (runs on window thread, be careful with accessing game engine data that might be in use in render thread!)
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (engineCore == nullptr) return 0;

	ImGuiInputBlock blockedInputs = ParseImGuiInputs(hwnd, message, wParam, lParam);

	switch (message)
	{
	case WM_PAINT:
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
		// The default window procedure will play a system notification sound 
		// when pressing the Alt+Enter keyboard combination if this message is 
		// not handled.
	case WM_SYSCHAR:
		return 0;
	case WM_SIZE:
	{
		RECT clientRect = {};
		::GetClientRect(hwnd, &clientRect);

		updateDataMutex.lock();
		updateData.resize = true;
		updateData.resize_width = clientRect.right - clientRect.left;
		updateData.resize_height = clientRect.bottom - clientRect.top;
		updateDataMutex.unlock();

		return 0;
	}
	case WM_SYSKEYDOWN:
		if ((wParam == VK_RETURN) && (lParam & (1 << 29)))
		{
			updateDataMutex.lock();
			updateData.toggleWindowMode = true;
			updateDataMutex.unlock();
			return 0;
		}
		break;
	case WM_KEYDOWN:
		if (!blockedInputs.blockKeyboard)
		{
			EngineInput& input = engineCore->m_game->GetInput();
			input.accessMutex.lock();
			input.currentPressedKeys->Set(wParam);
			input.accessMutex.unlock();
		}
	case WM_KEYUP:
		if (!blockedInputs.blockKeyboard)
		{
			if (wParam == VK_ESCAPE)
			{
				PostQuitMessage(0);
			}
		}
		return 0;
	}

	// Handle any messages the switch statement didn't.
	return DefWindowProc(hwnd, message, wParam, lParam);
}

// main
int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
#if defined(_DEBUG)
	DWORD fileWatcherThreadID;
	HANDLE fileWatcherThreadHandle = CreateThread(0, 0, FileWatcherThread, 0, 0, &fileWatcherThreadID);
#endif

	MSG msg = {};
	{
		// Create game and engine on window thread to set up events, then give it to render thread and never touch it again.
		Game game{ std::wstring(L"cool triangle") };

		EngineCore engine(1920, 1080, static_cast<IGame*>(&game));
		engineCore = &engine;
		engine.OnInit(hInstance, nCmdShow, WndProc);

		GameThreadData data{&engine};
		DWORD renderThreadID;
		HANDLE renderThreadHandle = CreateThread(0, 0, GameRenderThread, &data, 0, &renderThreadID);

		bool waitingForQuit = false;
		bool finishedQuit = false;
		while (!finishedQuit)
		{
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)
				{
					waitingForQuit = true;

					updateDataMutex.lock();
					updateData.quit = true;
					updateDataMutex.unlock();
				}

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			if (waitingForQuit)
			{
				WaitForSingleObject(renderThreadHandle, INFINITE);
				finishedQuit = true;
			}
		}
	}

#if defined(_DEBUG)
	CloseHandle(fileWatcherThreadHandle);

	{
		ComPtr<IDXGIDebug1> dxgiDebug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
		{
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		}
	}
#endif

	// Return this part of the WM_QUIT message to Windows.
	return static_cast<char>(msg.wParam);
}
