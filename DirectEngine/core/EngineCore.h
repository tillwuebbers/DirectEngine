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

#include "Input.h"
#include "ShadowMap.h"
#include "Texture.h"
#include "Audio.h"

#include "IGame.h"

#include "../imgui/imgui.h"
#include "../imgui/ProfilerTask.h"

#include "UI.h"

#include "../Helpers.h"
#include "Constants.h"
#include "Common.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

typedef XMMATRIX MAT_CMAJ;
typedef XMMATRIX MAT_RMAJ;

#define GAME_CREATION_PARAMS MemoryArena& globalArena, MemoryArena& configArena, MemoryArena& levelArena
typedef IGame* (*CreateGameFunc)(GAME_CREATION_PARAMS);

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
    CAMERA = 4,
    SHADOWMAP = 5,
    CUSTOM_START = 6,
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
    XMVECTOR time;
    float padding[(256 - 16) / 4];
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

struct CameraConstantBuffer
{
	MAT_CMAJ cameraView = {};
	MAT_CMAJ cameraProjection = {};
	XMVECTOR postProcessing = {};
	XMVECTOR fogColor = {};
	XMVECTOR worldCameraPos = {};
	float padding[(256 - 2 * 64 - 3 * 16) / 4];
};
static_assert((sizeof(CameraConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct BoneMatricesBuffer
{
    MAT_CMAJ inverseJointBinds[MAX_BONES];
    MAT_CMAJ jointTransforms[MAX_BONES];
};
static_assert((sizeof(BoneMatricesBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct ExtendedMatrix
{
    MAT_RMAJ matrix = XMMatrixIdentity();
    MAT_CMAJ matrixT = XMMatrixIdentity();
    MAT_RMAJ inverse = XMMatrixIdentity();
    MAT_CMAJ inverseT = XMMatrixIdentity();

    XMVECTOR translation = {};
    XMVECTOR rotation = XMQuaternionIdentity();
    XMMATRIX rotationMatrix = XMMatrixIdentity();
    XMVECTOR scale = { 1.f, 1.f, 1.f };

    XMVECTOR right = {};
    XMVECTOR up = {};
    XMVECTOR forward = {};


    void SetMatrix(const MAT_RMAJ mat)
    {
        matrix = mat;
        matrixT = XMMatrixTranspose(matrix);
        XMMatrixDecompose(&scale, &rotation, &translation, matrix);
        
        rotationMatrix = XMMatrixRotationQuaternion(rotation);
        right   = XMVector3Transform(V3_RIGHT,   rotationMatrix);
        up      = XMVector3Transform(V3_UP,      rotationMatrix);
        forward = XMVector3Transform(V3_FORWARD, rotationMatrix);

        inverse = XMMatrixInverse(nullptr, matrix);
        inverseT = XMMatrixTranspose(inverse);
    }
};

struct EntityData
{
    bool visible = true;
    bool wireframe = false;
    bool mainOnly = false;
    size_t entityIndex = 0;
    size_t materialIndex = 0;
    ConstantBuffer<EntityConstantBuffer> constantBuffer = {};
    ConstantBuffer<BoneMatricesBuffer> boneConstantBuffer = {};
    ConstantBuffer<BoneMatricesBuffer> firstPersonBoneConstantBuffer = {};
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
};

struct MaterialData
{
    size_t vertexCount = 0;
    size_t maxVertexCount = 0;
    size_t vertexStride = 0;
    ID3D12Resource* vertexUploadBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    CountingArray<EntityData*, MAX_ENTITIES_PER_MATERIAL> entities = {};
    CountingArray<Texture*, MAX_TEXTURES_PER_MATERIAL> textures = {};
    PipelineConfig* pipeline = nullptr;
    std::string name = "Material";
};

class DebugLineData
{
public:
    DebugLineData(MemoryArena& arena) : lineVertices(arena, MAX_DEBUG_LINE_VERTICES) {}

    FixedList<Vertex> lineVertices;
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

        lineVertices.newElement() = { startPos3, startCol4 };
		lineVertices.newElement() = { endPos3, endCol4 };
    }
};

struct CameraData
{
    ConstantBuffer<CameraConstantBuffer> constantBuffer = {};
    ExtendedMatrix worldMatrix = {};
    float fovY = 45.f;
    float aspectRatio = 1.f;
    float nearClip = .1f;
    float farClip = 100.f;
    bool skipRenderTextures = false;
    FixedStr name = "Camera";

    void UpdateViewMatrix(MAT_RMAJ& cameraEntityWorldMatrix)
    {
        worldMatrix.SetMatrix(cameraEntityWorldMatrix);
        constantBuffer.data.worldCameraPos = worldMatrix.translation;
        constantBuffer.data.cameraView = XMMatrixTranspose(worldMatrix.inverse);
    }

    void UpdateProjectionMatrix()
    {
        constantBuffer.data.cameraProjection = XMMatrixTranspose(XMMatrixPerspectiveFovLH(fovY, aspectRatio, nearClip, farClip));
    }
};

struct RenderTexture
{
    UINT width;
    UINT height;
    DXGI_FORMAT format;
    CD3DX12_VIEWPORT viewport;
    CD3DX12_RECT scissorRect;

    CameraData* camera;

    Texture texture{};
    ID3D12Resource* msaaBuffer;
    ID3D12Resource* dsvBuffer;
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
};

class EngineCore
{
public:
    static const UINT FrameCount = 3;

    EngineCore(UINT width, UINT height, CreateGameFunc gameFunc);

    void OnInit(HINSTANCE hInst, int nCmdShow, WNDPROC wndProc);
    void OnResize(UINT width, UINT height);
    void OnShaderReload();
    void OnUpdate();
    void OnRender();
    void OnDestroy();

    IGame* m_game = nullptr;
    bool m_gameStarted = false;
    bool m_quit = false;
    bool m_resetLevel = false;

    ComStack comPointers = {};
    ComStack comPointersSizeDependent = {};
    ComStack comPointersTextureUpload = {};
    ComStack comPointersLevel = {};
    MemoryArena engineArena = {};
    MemoryArena configArena = {};
    MemoryArena levelArena = {};
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
    ID3D12Resource* m_msaaRenderTargets[FrameCount] = {};
    ID3D12Resource* m_depthStencilBuffer = nullptr;
    ID3D12Resource* m_msaaDepthStencilBuffer = nullptr;
    ID3D12GraphicsCommandList* m_uploadCommandList = nullptr;
    ID3D12GraphicsCommandList* m_renderCommandList = nullptr;
    bool m_scheduleUpload = false;
    bool m_renderTextureEnabled = true;
    bool m_msaaEnabled = true;
    UINT m_msaaSampleCount = 4;
    PipelineConfig* m_shadowConfig;
    PipelineConfig* m_wireframeConfig;
    PipelineConfig* m_debugLineConfig;
    
    D3D12_CPU_DESCRIPTOR_HANDLE m_swapchainRtvHandles[FrameCount];
    D3D12_CPU_DESCRIPTOR_HANDLE m_swapchainDsvHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE m_msaaRtvHandles[FrameCount];
    D3D12_CPU_DESCRIPTOR_HANDLE m_msaaDsvHandle;
    UINT m_rtvDescriptorSize;
    UINT m_dsvDescriptorSize;

    UINT m_constantBufferCount = 0;
    UINT m_depthStencilViewCount = 2; // main dsv, msaa dsv are fixed
    UINT m_renderTargetViewCount = FrameCount * 2; // main rtv, msaa rtv

    // App resources
    ConstantBuffer<SceneConstantBuffer> m_sceneConstantBuffer = {};
    ConstantBuffer<LightConstantBuffer> m_lightConstantBuffer = {};
    ShadowMap* m_shadowmap = nullptr;
    ID3D12Resource* m_textureUploadHeaps[MAX_TEXTURE_UPLOADS] = {};
    size_t m_textureUploadIndex = 0;
    FixedList<MaterialData> m_materials{ engineArena, MAX_MATERIALS };
    FixedList<Texture> m_textures = { engineArena, MAX_TEXTURE_UPLOADS };
    FixedList<CameraData> m_cameras = { engineArena, MAX_CAMERAS };
    std::unordered_map<uint64_t, D3D12_VERTEX_BUFFER_VIEW> m_meshes{};

    ImGuiUI m_imgui = {};
    CameraData* mainCamera = nullptr;
    FixedList<RenderTexture*> m_renderTextures{ engineArena, 2 };

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
    DebugLineData m_debugLineData{ engineArena };

    // TODO: don't init this in game
    D3D12_VERTEX_BUFFER_VIEW cubeVertexView;
    const float m_renderTargetClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    
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
    void LoadAssets();
    void ResetLevelData();
    CameraData* CreateCamera();
    CD3DX12_CPU_DESCRIPTOR_HANDLE CreateDepthStencilView(UINT width, UINT height, ComStack& comStack, ID3D12Resource** bufferTarget, int fixedOffset = -1, UINT sampleCount = 1);
    void CreatePipeline(PipelineConfig* config, size_t constantBufferCount, size_t rootConstantCount);
    void CreatePipelineState(PipelineConfig* config, bool hotloadShaders = false);
    RenderTexture* CreateRenderTexture(UINT width, UINT height);
    Texture* CreateTexture(const std::wstring& filePath);
    void UploadTexture(const TextureData& textureData, std::vector<D3D12_SUBRESOURCE_DATA>& subresources, Texture& targetTexture);
    size_t CreateMaterial(const size_t maxVertices, const size_t vertexStride, const std::vector<Texture*>& textures, const std::wstring& shaderName);
    D3D12_VERTEX_BUFFER_VIEW CreateMesh(const size_t materialIndex, const void* vertexData, const size_t vertexCount, const uint64_t id);
    size_t CreateEntity(const size_t materialIndex, D3D12_VERTEX_BUFFER_VIEW& meshIndex);
    void UploadVertices();
    void RenderShadows(ID3D12GraphicsCommandList* renderList);
    void RenderScene(ID3D12GraphicsCommandList* renderList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, CameraData* camera);
    void RenderWireframe(ID3D12GraphicsCommandList* renderList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, CameraData* camera);
    void RenderDebugLines(ID3D12GraphicsCommandList* renderList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, CameraData* camera);
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

    void Transition(ID3D12GraphicsCommandList* renderList, ID3D12Resource* resource, D3D12_RESOURCE_STATES from, D3D12_RESOURCE_STATES to)
    {
        CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, from, to);
        renderList->ResourceBarrier(1, &barrier);
    }

    DescriptorHandle GetNewDescriptorHandle()
    {
        assert(m_constantBufferCount < m_cbvHeap->GetDesc().NumDescriptors);

        UINT incrementSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHeapEntry(m_cbvHeap->GetCPUDescriptorHandleForHeapStart(), m_constantBufferCount, incrementSize);
        CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHeapEntry(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), m_constantBufferCount, incrementSize);

        m_constantBufferCount++;
        return { cpuHeapEntry, gpuHeapEntry };
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetNewRTVHandle()
    {
        assert(m_renderTargetViewCount < m_rtvHeap->GetDesc().NumDescriptors);

        UINT incrementSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHeapEntry(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_renderTargetViewCount, incrementSize);

		m_renderTargetViewCount++;
		return cpuHeapEntry;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE GetNewDSVHandle()
    {
        assert(m_depthStencilViewCount < m_depthStencilHeap->GetDesc().NumDescriptors);

        UINT incrementSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHeapEntry(m_depthStencilHeap->GetCPUDescriptorHandleForHeapStart(), m_depthStencilViewCount, incrementSize);

        m_depthStencilViewCount++;
        return cpuHeapEntry;
    }

    template<typename T>
    void CreateConstantBuffers(ConstantBuffer<T>& buffers, const wchar_t* name, ComStack* comStack = nullptr)
    {
        ComStack* stack = comStack ? comStack : &comPointers;

        const UINT constantBufferSize = sizeof(T);
        assert(constantBufferSize % 256 == 0);

        assert(FrameCount <= MAX_FRAME_QUEUE);
        for (int i = 0; i < FrameCount; i++)
        {
            CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
            CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize);
            ThrowIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, NewComObject(*stack, &buffers.resources[i])));

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