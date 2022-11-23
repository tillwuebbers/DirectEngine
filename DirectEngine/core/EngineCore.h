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
#include "ShadowMap.h"
#include "Texture.h"

#include "../game/IGame.h"
#include "../game/Mesh.h"

#include "../imgui/imgui.h"
#include "../imgui/ProfilerTask.h"

#include "../Helpers.h"
#include "Constants.h"
#include "Common.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

enum class WindowMode
{
    Fullscreen,
    Borderless,
    Windowed
};

enum RootSignatureOffset : UINT
{
    SCENE = 0,
    LIGHT = 1,
    ENTITY = 2,
    SHADOWMAP = 3,
    CUSTOM_START = 4,
};

struct FrameDebugData
{
    float duration;
};

template<typename T>
struct ConstantBuffer
{
    ComPtr<ID3D12Resource> resources[MAX_FRAME_QUEUE] = {};
    DescriptorHandle handles[MAX_FRAME_QUEUE] = {};
    uint8_t* mappedData[MAX_FRAME_QUEUE] = {};
    T data;

    void UploadData(UINT frameIndex)
    {
        memcpy(mappedData[frameIndex], &data, sizeof(data));
    }
};

struct SceneConstantBuffer
{
    XMMATRIX cameraView = {};
    XMMATRIX cameraProjection = {};
    XMVECTOR postProcessing = {};
    XMVECTOR worldCameraPos = {};
    XMVECTOR time;
    float padding[18];
};
static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct LightConstantBuffer
{
    XMMATRIX lightView = {};
    XMMATRIX lightProjection = {};
    XMVECTOR sunDirection = {};
    float padding[26];
};
static_assert((sizeof(LightConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct EntityConstantBuffer
{
    XMMATRIX worldTransform = {};
    XMVECTOR color = { 1., 1., 1. };
    XMVECTOR aabbLocalPosition = { 0., 0., 0. };
    XMVECTOR aabbLocalSize = { 1., 1., 1. };
    bool isSelected;
    float padding[(256 - 64 - 3 * 16 - 16) / 4];
};
const int debugbuffersize = sizeof(EntityConstantBuffer) % 256;
static_assert((sizeof(EntityConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct EntityData
{
    bool visible = true;
    bool wireframe = false;
    XMVECTOR aabbLocalPosition = { 0., 0., 0. };
    XMVECTOR aabbLocalSize = { 1., 1., 1. };
    size_t entityIndex = 0;
    size_t materialIndex = 0;
    ConstantBuffer<EntityConstantBuffer> constantBuffer = {};
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
};

struct MaterialData
{
    size_t vertexCount = 0;
    size_t maxVertexCount = 0;
    size_t vertexStride = 0;
    ComPtr<ID3D12Resource> vertexUploadBuffer;
    ComPtr<ID3D12Resource> vertexBuffer;
    std::vector<EntityData> entities{};
    std::vector<Texture*> textures{};
    PipelineConfig* pipeline;
};

class EngineCore
{
public:
    static const UINT FrameCount = 3;

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

    MemoryArena engineArena = {};
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
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_cbvHeap = nullptr;
    ComPtr<ID3D12DescriptorHeap> m_depthStencilHeap = nullptr;
    ComPtr<ID3D12GraphicsCommandList> m_uploadCommandList = {};
    ComPtr<ID3D12GraphicsCommandList> m_renderCommandList = {};
    bool m_scheduleUpload = false;
    UINT m_rtvDescriptorSize;
    UINT m_dsvDescriptorSize;
    PipelineConfig* m_shadowConfig;
    PipelineConfig* m_wireframeConfig;

    // App resources
    ConstantBuffer<SceneConstantBuffer> m_sceneConstantBuffer = {};
    ConstantBuffer<LightConstantBuffer> m_lightConstantBuffer = {};
    ComPtr<ID3D12Resource> m_depthStencilBuffer = nullptr;
    ShadowMap* m_shadowmap = nullptr;
    ComPtr<ID3D12Resource> m_debugTexture = nullptr;
    std::vector<ComPtr<ID3D12Resource>> m_textureUploadHeaps = {};
    std::vector<MaterialData> m_materials = {};

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

    // TODO: don't init this in game
    D3D12_VERTEX_BUFFER_VIEW cubeVertexView;
    bool renderAABB = false;
    
    // Time
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_frameStartTime;
    double m_updateDeltaTime;

    void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName, WNDPROC wndProc);
    void CreateGameWindow(const wchar_t* windowClassName, HINSTANCE hInst, uint32_t width, uint32_t height);
    void GetHardwareAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter);
    void LoadPipeline();
    void LoadSizeDependentResources();
    PipelineConfig* CreatePipeline(ShaderDescription shaderDesc, size_t textureCount, bool wireframe);
    void CreatePipelineState(PipelineConfig* config);
    void LoadAssets();
    void CreateTexture(Texture& outTexture, const wchar_t* filePath, const wchar_t* debugName);
    void UploadTexture(const TextureData& textureData, std::vector<D3D12_SUBRESOURCE_DATA>& subresources, Texture& targetTexture, const wchar_t* name);
    size_t CreateMaterial(const size_t maxVertices, const size_t vertexStride, std::vector<Texture*> textures, ShaderDescription shaderDesc);
    D3D12_VERTEX_BUFFER_VIEW CreateMesh(const size_t materialIndex, const void* vertexData, const size_t vertexCount);
    size_t CreateEntity(const size_t materialIndex, D3D12_VERTEX_BUFFER_VIEW& meshIndex);
    void UploadVertices();
    CD3DX12_GPU_DESCRIPTOR_HANDLE* GetConstantBufferHandle(int offset);
    void RenderShadows(ID3D12GraphicsCommandList* renderList);
    void RenderScene(ID3D12GraphicsCommandList* renderList);
    void RenderWireframe(ID3D12GraphicsCommandList* renderList);
    void PopulateCommandList();
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

    void Transition(ID3D12GraphicsCommandList* renderList, ID3D12Resource* resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, from, to);
        renderList->ResourceBarrier(1, &barrier);
    }

    DescriptorHandle GetNewDescriptorHandle()
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHeapStart = m_cbvHeap->GetCPUDescriptorHandleForHeapStart();
        CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHeapEntry{};
        cpuHeapEntry.InitOffsetted(cpuHeapStart, cbcount * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

        D3D12_GPU_DESCRIPTOR_HANDLE gpuHeapStart = m_cbvHeap->GetGPUDescriptorHandleForHeapStart();
        CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHeapEntry{};
        gpuHeapEntry.InitOffsetted(gpuHeapStart, cbcount * m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));

        cbcount++;
        return { cpuHeapEntry, gpuHeapEntry };
    }

    template<typename T>
    void CreateConstantBuffers(ConstantBuffer<T>& buffers, const wchar_t* name)
    {
        const UINT constantBufferSize = sizeof(T);
        assert(constantBufferSize % 256 == 0);

        assert(FrameCount <= MAX_FRAME_QUEUE);
        for (int i = 0; i < FrameCount; i++)
        {
            CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
            ThrowIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffers.resources[i])));

            // Describe and create a constant buffer view.
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = buffers.resources[i]->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = constantBufferSize;
            
            buffers.handles[i] = GetNewDescriptorHandle();
            m_device->CreateConstantBufferView(&cbvDesc, buffers.handles[i].cpuHandle);

            // Map and initialize the constant buffer. We don't unmap this until the
            // app closes. Keeping things mapped for the lifetime of the resource is okay.
            CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
            ThrowIfFailed(buffers.resources[i]->Map(0, &readRange, reinterpret_cast<void**>(&buffers.mappedData[i])));

            buffers.resources[i]->SetName(std::format(L"{} {}", name, i).c_str());
        }
    }
};

double NanosecondsToSeconds(std::chrono::nanoseconds clock);