#pragma once

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <chrono>
#include <unordered_map>

#include "../game/Game.h"
#include "../imgui/ProfilerTask.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

enum class WindowMode
{
    Fullscreen,
    Borderless,
    Windowed
};

struct FrameDebugData
{
    float duration;
};

struct SceneConstantBuffer
{
    XMFLOAT4 time;
    float padding[60]; // Padding so the constant buffer is 256-byte aligned.
};
static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

class EngineCore
{
public:
    static const UINT FrameCount = 3;

    EngineCore(UINT width, UINT height, Game* game);

    LRESULT CALLBACK ProcessWindowMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void OnInit(HINSTANCE hInst, int nCmdShow, WNDPROC wndProc);
    void OnUpdate();
    void OnRender();
    void OnDestroy();

    Game* m_game;

    // Window Handle
    HWND m_hwnd;
    std::wstring m_windowName;

    // Pipeline objects
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain = nullptr;
    ComPtr<ID3D12Device> m_device = nullptr;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
    ComPtr<ID3D12CommandQueue> m_commandQueue = nullptr;
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap = nullptr;
    ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
    ComPtr<ID3D12GraphicsCommandList> m_commandList = nullptr;
    UINT m_rtvDescriptorSize;

    // App resources
    ComPtr<ID3D12Resource> m_vertexBuffer = nullptr;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    ComPtr<ID3D12Resource> m_constantBuffer = nullptr;
    SceneConstantBuffer m_constantBufferData;
    UINT8* m_pCbvDataBegin = nullptr;

    // Synchronization objects
    UINT m_frameIndex = 0;
    HANDLE m_fenceEvent = nullptr;
    ComPtr<ID3D12Fence> m_fence = nullptr;
    UINT64 m_fenceValues[FrameCount];
    //HANDLE m_frameLatencyWaitableObject = nullptr;

    // Viewport dimensions
    UINT m_width;
    UINT m_height;
    float m_aspectRatio;

    // Adapter info
    bool m_useWarpDevice = false;
    
    // Window Settings
    bool m_tearingSupport = false;
    WindowMode m_windowMode = WindowMode::Windowed;
    WindowMode m_wantedWindowMode = WindowMode::Windowed;
    bool m_wantBorderless = false;
    bool m_useVsync = true;
    RECT m_windowRect;
    const UINT m_windowStyle = WS_OVERLAPPEDWINDOW;

    // Debug stuff
    bool m_wantReloadShaders = false;
    std::string m_shaderError = {};
    FrameDebugData m_lastFrames[256] = {};
    size_t m_lastDebugFrameIndex = 0;
    bool m_pauseDebugFrames = false;
    std::vector<legit::ProfilerTask> m_profilerTaskData{};
    std::unordered_map<std::string, size_t> m_profilerTasks{};
    
    // Time
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_frameStartTime;
    double m_updateDeltaTime;

    void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName, WNDPROC wndProc);
    void CreateGameWindow(const wchar_t* windowClassName, HINSTANCE hInst, uint32_t width, uint32_t height);
    void GetHardwareAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter);
    void LoadPipeline();
    void LoadSizeDependentResources();
    HRESULT CreatePipelineState();
    void LoadAssets();
    void PopulateCommandList();
    void MoveToNextFrame();
    void WaitForGpu();
    void OnResize();
    void CheckTearingSupport();
    void ToggleWindowMode();
    void ApplyWindowMode(WindowMode newMode);
    void BeginProfile(std::string name, ImColor color);
    void EndProfile(std::string name);
    double TimeSinceStart();
    double TimeSinceFrameStart();
};

double NanosecondsToSeconds(std::chrono::nanoseconds clock);