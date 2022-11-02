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
#include "../game/Mesh.h"

#include "../imgui/imgui.h"
#include "../imgui/ProfilerTask.h"

#include "../Helpers.h"

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

struct CommandList
{
    ComPtr<ID3D12GraphicsCommandList> list = nullptr;
    bool scheduled = false;

    void Reset(ID3D12CommandAllocator* allocator, ID3D12PipelineState* pipelineState);
};

struct SceneConstantBuffer
{
    XMMATRIX cameraTransform = {};
    XMMATRIX clipTransform = {};
    float time;
    float deltaTime;
    float padding[30]; // Padding so the constant buffer is 256-byte aligned.
};
static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct EntityConstantBuffer
{
    XMMATRIX worldTransform[8] = {};
    XMVECTOR color[8];
    XMVECTOR isSelected[8];
    //float padding[0];
};
const int debugbuffersize = sizeof(EntityConstantBuffer) % 256;
static_assert((sizeof(EntityConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct DrawCallData
{
    size_t vertexCount = 0;
    size_t maxVertexCount = 0;
    size_t vertexStride = 0;
    int descriptorOffset = 0;
    ComPtr<ID3D12Resource> vertexUploadBuffer;
    ComPtr<ID3D12Resource> vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
    std::vector<ComPtr<ID3D12Resource>> constantBuffers = {};
    std::vector<UINT8*> mappedConstantBufferData = {};
    EntityConstantBuffer constantBufferData = {};
};

struct EntityData
{
    bool visible = true;
    size_t entityIndex;
    size_t drawCallIndex;
};

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

    IGame* m_game = nullptr;
    bool m_gameStarted = false;
    bool m_quit = false;

    MemoryArena frameArena = {};

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
    CommandList m_uploadCommandList = {};
    CommandList m_renderCommandList = {};
    std::vector<CommandList*> m_scheduledCommandLists = {};
    UINT m_rtvDescriptorSize;

    // App resources
    std::vector<ComPtr<ID3D12Resource>> m_sceneConstantBuffers = {};
    ComPtr<ID3D12Resource> m_depthStencilBuffer = nullptr;
    SceneConstantBuffer m_sceneData;
    std::vector<UINT8*> m_mappedSceneData = {};
    std::vector<DrawCallData> m_drawCalls = {};
    std::vector<EntityData> m_entities = {};

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
    size_t CreateDrawCall(const size_t maxVertices, const size_t vertexStride);
    size_t CreateEntity(const size_t drawCallIndex, Vertex* vertexData, const size_t vertexCount);
    void FinishDrawCallSetup(size_t drawCallIndex);
    CD3DX12_GPU_DESCRIPTOR_HANDLE* GetConstantBufferHandle(int offset);
    void PopulateCommandList();
    void ScheduleCommandList(CommandList* newList);
    void MoveToNextFrame();
    void WaitForGpu();
    void CheckTearingSupport();
    void ToggleWindowMode();
    void ApplyWindowMode();
    void BeginProfile(std::string name, ImColor color);
    void EndProfile(std::string name);
    double TimeSinceStart();
    double TimeSinceFrameStart();

    // TODO: be smarter about this?
    UINT cbcount = 0;

    template<typename T>
    void CreateConstantBuffers(std::vector<ComPtr<ID3D12Resource>>& buffers, std::vector<UINT8*>& outMappedData)
    {
        const UINT constantBufferSize = sizeof(T);
        assert(constantBufferSize % 256 == 0);
        buffers.resize(FrameCount);
        outMappedData.resize(FrameCount);

        for (int i = 0; i < FrameCount; i++)
        {
            CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
            ThrowIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffers[i])));

            // Describe and create a constant buffer view.
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = buffers[i]->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = constantBufferSize;
            D3D12_CPU_DESCRIPTOR_HANDLE heapStart = m_cbvHeap->GetCPUDescriptorHandleForHeapStart();
            CD3DX12_CPU_DESCRIPTOR_HANDLE heapEntry{};
            heapEntry.InitOffsetted(heapStart, cbcount * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
            m_device->CreateConstantBufferView(&cbvDesc, heapEntry);

            // Map and initialize the constant buffer. We don't unmap this until the
            // app closes. Keeping things mapped for the lifetime of the resource is okay.
            CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
            ThrowIfFailed(buffers[i]->Map(0, &readRange, reinterpret_cast<void**>(&outMappedData[i])));

            cbcount++;
        }
    }
};

double NanosecondsToSeconds(std::chrono::nanoseconds clock);