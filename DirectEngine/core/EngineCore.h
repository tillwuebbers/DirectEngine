#pragma once

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
#include "ComStack.h"

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <chrono>
#include <unordered_map>
#include <array>

#include <xaudio2Redist.h>

#include "glm/mat4x4.hpp"

#include "Input.h"
#include "ShadowMap.h"
#include "Texture.h"
#include "Audio.h"

#include "../game/IGame.h"
#include "../game/Mesh.h"

#include "../imgui/imgui.h"
#include "../imgui/ProfilerTask.h"

#include "../Helpers.h"
#include "Constants.h"
#include "Common.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

typedef XMMATRIX MAT_CMAJ;
typedef XMMATRIX MAT_RMAJ;

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
    BONES = 3,
    XR = 4,
    SHADOWMAP = 5,
    CAM = 6,
    CUSTOM_START = 7,
};

struct FrameDebugData
{
    float duration;
};

template<typename T>
struct ConstantBuffer
{
    ID3D12Resource* resources[MAX_FRAME_QUEUE] = {};
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
    MAT_CMAJ cameraView = {};
    MAT_CMAJ cameraProjection = {};
    XMVECTOR postProcessing = {};
    XMVECTOR fogColor = {};
    XMVECTOR worldCameraPos = {};
    XMVECTOR time;
    float padding[14];
};
static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct LightConstantBuffer
{
    MAT_CMAJ lightView = {};
    MAT_CMAJ lightProjection = {};
    XMVECTOR sunDirection = {};
    float padding[26];
};
static_assert((sizeof(LightConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct EntityConstantBuffer
{
    MAT_CMAJ worldTransform = {};
    XMVECTOR color = { 1., 1., 1. };
    XMVECTOR aabbLocalPosition = { 0., 0., 0. };
    XMVECTOR aabbLocalSize = { 1., 1., 1. };
    bool isSelected;
    float padding[(256 - 64 - 3 * 16 - 16) / 4];
};
static_assert((sizeof(EntityConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct BoneMatricesBuffer
{
    MAT_CMAJ inverseJointBinds[MAX_BONES];
    MAT_CMAJ jointTransforms[MAX_BONES];
};
static_assert((sizeof(BoneMatricesBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct EntityData
{
    bool visible = true;
    bool wireframe = false;
    XMVECTOR aabbLocalPosition = { 0., 0., 0. };
    XMVECTOR aabbLocalSize = { 1., 1., 1. };
    size_t entityIndex = 0;
    size_t materialIndex = 0;
    ConstantBuffer<EntityConstantBuffer> constantBuffer = {};
    ConstantBuffer<BoneMatricesBuffer> boneConstantBuffer = {};
    size_t boneCount;
    TransformHierachy* transformHierachy;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
};

struct MaterialData
{
    size_t vertexCount = 0;
    size_t maxVertexCount = 0;
    size_t vertexStride = 0;
    ID3D12Resource* vertexUploadBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    EntityData* entities[MAX_ENTITIES_PER_MATERIAL] = {};
    size_t entityCount = 0;
    Texture* textures[MAX_TEXTURES_PER_MATERIAL] = {};
    size_t textureCount = 0;
    PipelineConfig* pipeline = nullptr;
};

class DebugLineData
{
public:
    ArenaList<Vertex> lineVertices;
    ID3D12Resource* vertexBuffer = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};

    void AddLine(XMVECTOR startPos, XMVECTOR endPos, XMVECTOR startColor = { 1., 1., 1., 1. }, XMVECTOR endColor = { 1., 1., 1., 1. })
    {
        XMFLOAT3 startPos3;
		XMFLOAT3 endPos3;
		XMStoreFloat3(&startPos3, startPos);
		XMStoreFloat3(&endPos3, endPos);

        XMFLOAT4 startCol4;
		XMFLOAT4 endCol4;
		XMStoreFloat4(&startCol4, startColor);
		XMStoreFloat4(&endCol4, endColor);

        *lineVertices.new_element() = { startPos3, startCol4 };
		*lineVertices.new_element() = { endPos3, endCol4 };
    }
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

    ComStack comPointers = {};
    ComStack comPointersSizeDependent = {};
    ComStack comPointersTextureUpload = {};
    MemoryArena engineArena = {};
    MemoryArena frameArena = {};

    // Window Handle
    HWND m_hwnd;
    std::wstring m_windowName = L"DirectEngine";

    // Pipeline objects
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;

    ID3D12Device* m_device = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    IDXGISwapChain3* m_swapChain = nullptr;
    ID3D12DescriptorHeap* m_rtvHeap = nullptr;
    ID3D12DescriptorHeap* m_cbvHeap = nullptr;
    ID3D12DescriptorHeap* m_depthStencilHeap = nullptr;
    ID3D12CommandAllocator* m_uploadCommandAllocators[FrameCount] = {};
    ID3D12CommandAllocator* m_renderCommandAllocators[FrameCount] = {};
    ID3D12Resource* m_renderTargets[FrameCount] = {};
    ID3D12Resource* m_depthStencilBuffer = nullptr;
    ID3D12GraphicsCommandList* m_uploadCommandList = nullptr;
    ID3D12GraphicsCommandList* m_renderCommandList = nullptr;
    bool m_scheduleUpload = false;
    PipelineConfig* m_shadowConfig;
    PipelineConfig* m_boneDebugConfig;
    PipelineConfig* m_wireframeConfig;
    PipelineConfig* m_debugLineConfig;
    
    D3D12_CPU_DESCRIPTOR_HANDLE m_swapchainRtvHandles[FrameCount];
    UINT m_rtvDescriptorSize;
    UINT m_dsvDescriptorSize;

    // App resources
    ConstantBuffer<SceneConstantBuffer> m_sceneConstantBuffer = {};
    ConstantBuffer<LightConstantBuffer> m_lightConstantBuffer = {};
    ShadowMap* m_shadowmap = nullptr;
    ID3D12Resource* m_textureUploadHeaps[MAX_TEXTURE_UPLOADS] = {};
    size_t m_textureUploadIndex = 0;
    MaterialData m_materials[MAX_MATERIALS] = {};
    size_t m_materialCount = 0;
	Texture m_textures[MAX_TEXTURE_UPLOADS] = {};
	size_t m_textureCount = 0;
    uint32_t cameraIndex = 0;

    // Synchronization objects
    UINT m_frameIndex = 0;
    HANDLE m_fenceEvent = nullptr;
    ID3D12Fence* m_fence = nullptr;
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
    DebugLineData m_debugLineData;

    // TODO: don't init this in game
    D3D12_VERTEX_BUFFER_VIEW cubeVertexView;
    bool renderAABB = false;
    bool renderBones = false;
    
    // Audio
    IXAudio2* m_audio;
    X3DAUDIO_HANDLE m_3daudio;
    IXAudio2MasteringVoice* m_audioMasteringVoice;
    XAUDIO2_VOICE_DETAILS m_audioVoiceDetails;
    X3DAUDIO_DSP_SETTINGS m_audioDspSettingsMono{};
    X3DAUDIO_DSP_SETTINGS m_audioDspSettingsStereo{};
    WAVEFORMATEX* m_defaultAudioFormat;

    // Time
    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_frameStartTime;
    double m_updateDeltaTime;

    void RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName, WNDPROC wndProc);
    void CreateGameWindow(const wchar_t* windowClassName, HINSTANCE hInst, uint32_t width, uint32_t height);
    void LoadPipeline(LUID* requiredLuid);
    void LoadSizeDependentResources();
    void CreatePipeline(PipelineConfig* config, size_t constantBufferCount, size_t rootConstantCount);
    void CreatePipelineState(PipelineConfig* config);
    void LoadAssets();
    Texture* CreateTexture(const wchar_t* filePath);
    void InitGPUTexture(Texture& outTexture, DXGI_FORMAT format, UINT width, UINT height, const wchar_t* name);
    void UploadTexture(const TextureData& textureData, std::vector<D3D12_SUBRESOURCE_DATA>& subresources, Texture& targetTexture);
    size_t CreateMaterial(const size_t maxVertices, const size_t vertexStride, std::vector<Texture*> textures, ShaderDescription shaderDesc);
    D3D12_VERTEX_BUFFER_VIEW CreateMesh(const size_t materialIndex, const void* vertexData, const size_t vertexCount);
    size_t CreateEntity(const size_t materialIndex, D3D12_VERTEX_BUFFER_VIEW& meshIndex, size_t boneCount, TransformHierachy* hierachy);
    void UploadVertices();
    void RenderShadows(ID3D12GraphicsCommandList* renderList);
    void RenderScene(ID3D12GraphicsCommandList* renderList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
    void RenderWireframe(ID3D12GraphicsCommandList* renderList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
    void RenderBones(ID3D12GraphicsCommandList* renderList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
    void RenderDebugLines(ID3D12GraphicsCommandList* renderList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle);
    void ExecCommandList(ID3D12GraphicsCommandList* commandList);
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
            ThrowIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, NewComObject(comPointers, &buffers.resources[i])));

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