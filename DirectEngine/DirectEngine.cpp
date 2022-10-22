#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d12.h>
#include <DirectXMath.h>
#include <dxgidebug.h>

#include "core/EngineCore.h"
#include "game/Game.h"
#include "DirectEngine.h"

EngineCore* engineCore;

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
			if (engineCore != nullptr)
			{
				engineCore->m_wantReloadShaders = true;
			}

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

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (engineCore == nullptr) return 0;

	return engineCore->ProcessWindowMessage(hwnd, message, wParam, lParam);
}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
#if defined(_DEBUG)
	DWORD fileWatcherThreadID;
	HANDLE fileWatcherThreadHandle = CreateThread(0, 0, FileWatcherThread, 0, 0, &fileWatcherThreadID);
#endif

	Game game{ std::wstring(L"cool triangle") };
	MSG msg = {};
	{
		EngineCore engine{ 1920, 1080, &game };
		engineCore = &engine;
		engine.OnInit(hInstance, nCmdShow, WndProc);

		bool wantsQuit = false;
		while (!wantsQuit)
		{
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT) wantsQuit = true;

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}

			engine.BeginProfile("Wait1", ImColor(1.f, 1.f, 1.f));
			//WaitForSingleObjectEx(engine.m_frameLatencyWaitableObject, 1000, true);
			engine.EndProfile("Wait1");
			engine.OnUpdate();
			engine.OnRender();
		}

		engine.OnDestroy();
		engineCore = nullptr;
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
