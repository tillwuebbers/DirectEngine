#pragma once

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <chrono>
#include <unordered_map>

#include "glm/mat4x4.hpp"

#include "Input.h"

#include "../game/IGame.h"

#include "../imgui/imgui.h"
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

struct MeshData
{
    UINT vertexCount = 0;
    UINT indexCount = 0;
    UINT instanceCount = 1;
    ComPtr<ID3D12Resource> vertexBuffer;
    ComPtr<ID3D12Resource> indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
};

struct SceneConstantBuffer
{
    XMMATRIX modelTransform = {};
    XMMATRIX cameraTransform = {};
    XMMATRIX clipTransform = {};
    float time;
    float deltaTime;
    float padding[14]; // Padding so the constant buffer is 256-byte aligned.
};
static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

class EngineCore
{
public:
    static const UINT FrameCount = 3;
    static const DXGI_FORMAT DisplayFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    EngineCore(UINT width, UINT height, IGame* game);

    void OnInit(HINSTANCE hInst, int nCmdShow, WNDPROC wndProc);
    void OnResize(UINT width, UINT height);
    void OnShaderReload();
    void OnUpdate();
    void OnRender();
    void OnDestroy();

    IGame* m_game;

    // Window Handle
    HWND m_hwnd;
    std::wstring m_windowName = L"DirectEngine";

    // Pipeline objects
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain = nullptr;
    ComPtr<ID3D12Device> m_device = nullptr;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_uploadCommandAllocators[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_renderCommandAllocators[FrameCount];
    ComPtr<ID3D12CommandQueue> m_commandQueue = nullptr;
    ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_depthStencilHeap = nullptr;
    ComPtr<ID3D12PipelineState> m_pipelineState = nullptr;
    ComPtr<ID3D12GraphicsCommandList> m_uploadCommandList = nullptr;
    ComPtr<ID3D12GraphicsCommandList> m_renderCommandList = nullptr;
    std::vector<ID3D12CommandList*> m_scheduledCommandLists = {};
    UINT m_rtvDescriptorSize;

    // App resources
    ComPtr<ID3D12Resource> m_uploadBuffer = nullptr;
    ComPtr<ID3D12Resource> m_constantBuffer = nullptr;
    ComPtr<ID3D12Resource> m_depthStencilBuffer = nullptr;
    SceneConstantBuffer m_constantBufferData;
    UINT8* m_pCbvDataBegin = nullptr;
    std::vector<MeshData> m_meshes = {};

    // Synchronization objects
    UINT m_frameIndex = 0;
    HANDLE m_fenceEvent = nullptr;
    ComPtr<ID3D12Fence> m_fence = nullptr;
    UINT64 m_fenceValues[FrameCount];
    HANDLE m_frameWaitableObject;

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
    std::string m_shaderError = {};
    FrameDebugData m_lastFrames[256] = {};
    size_t m_lastDebugFrameIndex = 0;
    bool m_pauseDebugFrames = false;
    std::vector<legit::ProfilerTask> m_profilerTaskData{};
    std::unordered_map<std::string, size_t> m_profilerTasks{};
    bool m_inUpdate = false;
    
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
    void CreateMesh(const void* vertexData, const size_t vertexStride, const size_t vertexCount, const size_t instanceCount);
    void PopulateCommandList();
    void MoveToNextFrame();
    void WaitForGpu();
    void CheckTearingSupport();
    void ToggleWindowMode();
    void ApplyWindowMode(WindowMode newMode);
    void BeginProfile(std::string name, ImColor color);
    void EndProfile(std::string name);
    double TimeSinceStart();
    double TimeSinceFrameStart();
};

double NanosecondsToSeconds(std::chrono::nanoseconds clock);