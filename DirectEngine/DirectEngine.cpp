#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <iostream>
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

bool cursorVisible = true;

EngineCore* engineCore;

EngineUpdate updateData{};
std::mutex updateDataMutex;

MemoryArena wndProcArena{};

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

	while (!engine.m_quit)
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
			engine.m_quit = true;
		}
		updateDataMutex.unlock();
		engine.EndProfile("Mutex Read");

		engine.OnUpdate();

		engine.BeginProfile("Waitable", ImColor(.3f, .3f, .3f));
		WaitForSingleObjectEx(engine.m_frameWaitableObject, 1000, true);
		engine.EndProfile("Waitable");

		engine.m_inUpdate = true;
		engine.OnRender();
		engine.m_inUpdate = false;
	}

	engine.OnDestroy();
	engineCore = nullptr;

	return 0;
}

void SetInputDown(size_t button)
{
	EngineInput& input = engineCore->m_game->GetInput();
	input.accessMutex.lock();
	input.currentPressedKeys->keys[button] = true;
	input.keysDown->keys[button] = true;
	input.accessMutex.unlock();
}

void SetInputUp(size_t button)
{
	EngineInput& input = engineCore->m_game->GetInput();
	input.accessMutex.lock();
	input.currentReleasedKeys->keys[button] = true;
	input.keysDown->keys[button] = false;
	input.accessMutex.unlock();
}

// Window event callback (runs on window thread, be careful with accessing game engine data that might be in use in render thread!)
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (engineCore == nullptr) return 0;

	ImGuiInputBlock blockedInputs{};
	if (cursorVisible)
	{
		blockedInputs = ParseImGuiInputs(hwnd, message, wParam, lParam);
	}

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
		// Alt+Enter
		if ((wParam == VK_RETURN) && (lParam & (1 << 29)))
		{
			updateDataMutex.lock();
			updateData.toggleWindowMode = true;
			updateDataMutex.unlock();
			return 0;
		}
		break;
	case WM_KEYDOWN:
	{
		bool repeat = lParam & (1 << 30);
		if (!blockedInputs.blockKeyboard && !repeat) SetInputDown(wParam);
		return 0;
	}
	case WM_KEYUP:
		if (!blockedInputs.blockKeyboard) SetInputUp(wParam);
		return 0;
	case WM_LBUTTONDOWN:
		if (!blockedInputs.blockMouse) SetInputDown(VK_LBUTTON);
		return 0;
	case WM_LBUTTONUP:
		if (!blockedInputs.blockMouse) SetInputUp(VK_LBUTTON);
		return 0;
	case WM_RBUTTONDOWN:
		if (!blockedInputs.blockMouse) SetInputDown(VK_RBUTTON);
		return 0;
	case WM_RBUTTONUP:
		if (!blockedInputs.blockMouse) SetInputUp(VK_RBUTTON);
		return 0;
	case WM_MBUTTONDOWN:
		if (!blockedInputs.blockMouse) SetInputDown(VK_MBUTTON);
		return 0;
	case WM_MBUTTONUP:
		if (!blockedInputs.blockMouse) SetInputUp(VK_MBUTTON);
		return 0;
	case WM_XBUTTONDOWN:
	{
		WORD buttonType = HIWORD(wParam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
		if (!blockedInputs.blockMouse) SetInputDown(buttonType);
		return true;
	}
	case WM_XBUTTONUP:
	{
		WORD buttonType = HIWORD(wParam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
		if (!blockedInputs.blockMouse) SetInputUp(buttonType);
		return true;
	}
	case WM_INPUT:
		UINT dwSize;

		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
		LPBYTE lpb = NewArray(wndProcArena, BYTE, dwSize);
		if (lpb == NULL)
		{
			return 0;
		}

		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
		{
			OutputDebugString(TEXT("GetRawInputData does not return correct size !\n"));
		}

		RAWINPUT* raw = (RAWINPUT*)lpb;
		if (raw->header.dwType == RIM_TYPEMOUSE)
		{
			EngineInput& input = engineCore->m_game->GetInput();
			input.accessMutex.lock();
			input.mouseDeltaAccX += raw->data.mouse.lLastX;
			input.mouseDeltaAccY += raw->data.mouse.lLastY;
			input.accessMutex.unlock();
		}
		break;
	}

	wndProcArena.Reset();

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
		Game game{};

		EngineCore engine(1920, 1080, static_cast<IGame*>(&game));
		//EngineCore engine(2016, 2240, static_cast<IGame*>(&game));
		engineCore = &engine;
		engine.OnInit(hInstance, nCmdShow, WndProc);

		GameThreadData data{ &engine };
		DWORD renderThreadID;
		HANDLE renderThreadHandle = CreateThread(0, 0, GameRenderThread, &data, 0, &renderThreadID);

		RAWINPUTDEVICE Rid[1];
		Rid[0].usUsagePage = 0x01;          // HID_USAGE_PAGE_GENERIC
		Rid[0].usUsage = 0x02;              // HID_USAGE_GENERIC_MOUSE
		Rid[0].dwFlags = 0x00;// RIDEV_NOLEGACY;    // adds mouse and also ignores legacy mouse messages
		Rid[0].hwndTarget = 0;
		RegisterRawInputDevices(Rid, _countof(Rid), sizeof(Rid[0]));

		bool waitingForQuit = false;
		bool finishedQuit = false;
		while (!finishedQuit)
		{
			// Handle pending window messages
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

			// Handle game messages
			game.windowUdpateDataMutex.lock();
			if (game.windowUpdateData.updateCursor)
			{
				game.windowUpdateData.updateCursor = false;
				if (game.windowUpdateData.cursorClipped)
				{
					RECT clientRect;
					GetClientRect(engine.m_hwnd, &clientRect);
					MapWindowPoints(engine.m_hwnd, nullptr, reinterpret_cast<POINT*>(&clientRect), 2);

					if (!ClipCursor(&clientRect))
						OutputDebugString(std::format(L"cursor lock failed: {:x}", GetLastError()).c_str());
				}
				else
				{
					if (!ClipCursor(NULL))
						OutputDebugString(std::format(L"cursor unlock failed: {:x}", GetLastError()).c_str());
				}
				
				CURSORINFO info{ sizeof(CURSORINFO) };
				if (!GetCursorInfo(&info))
					OutputDebugString(std::format(L"failed to GetCursorInfo: {:x}", GetLastError()).c_str());

				if (info.flags == 0 && game.windowUpdateData.cursorVisible)
				{
					ShowCursor(true);
					cursorVisible = true;
				}
				else if (info.flags != 0 && !game.windowUpdateData.cursorVisible)
				{
					ShowCursor(false);
					cursorVisible = false;
				}
			}
			game.windowUdpateDataMutex.unlock();

			// Quit if wanted
			DWORD renderThreadStatus;
			GetExitCodeThread(renderThreadHandle, &renderThreadStatus);
			if (waitingForQuit)
			{
				if (renderThreadStatus == STILL_ACTIVE)
				{
					WaitForSingleObject(renderThreadHandle, 10000);
				}
				finishedQuit = true;
			}
			else if (renderThreadStatus != STILL_ACTIVE)
			{
				finishedQuit = true;
			}
		}
	}

#if defined(_DEBUG)
	CloseHandle(fileWatcherThreadHandle);

	{
		Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
		if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
		{
			dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		}
	}
#endif

	// Return this part of the WM_QUIT message to Windows.
	return static_cast<char>(msg.wParam);
}
