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
#include "Log.h"
#include "Vertex.h"

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
    IRRADIANCE = 6,
    DEFAULT_ROOT_SIG_COUNT,
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
    MAT_CMAJ worldTransform = XMMatrixIdentity();
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

struct MeshData
{
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
    D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
    ID3D12Resource* bottomLevelAccelerationStructure = {};
    ID3D12Resource* scratchResource = {};
};

struct EntityData
{
    bool visible = true;
    bool raytraceVisible = true;
    bool wireframe = false;
    bool mainOnly = false;
    size_t entityIndex = 0;
    size_t materialIndex = 0;
    ConstantBuffer<EntityConstantBuffer> constantBuffer = {};
    ConstantBuffer<BoneMatricesBuffer> boneConstantBuffer = {};
    ConstantBuffer<BoneMatricesBuffer> firstPersonBoneConstantBuffer = {};
    MeshData* meshData;
};

enum class RootConstantType
{
    UINT = 0,
    FLOAT = 1,
};

struct RootConstantInfo
{
    RootConstantType type;
    FixedStr name = "-";
};

class MaterialData
{
public:
    CountingArray<EntityData*, MAX_ENTITIES_PER_MATERIAL> entities = {};
    CountingArray<Texture*, MAX_TEXTURES_PER_MATERIAL> textures = {};
    CountingArray<RootConstantInfo, MAX_ROOT_CONSTANTS_PER_MATERIAL> rootConstants = {};
    UINT rootConstantData[MAX_ROOT_CONSTANTS_PER_MATERIAL] = {};
    PipelineConfig* pipeline = nullptr;
    std::string name = "Material";
    size_t shellCount = 0;

    template <typename T>
    void SetRootConstant(size_t index, T value)
    {
        assert(index < rootConstants.size);
        T* rootConstAddr = reinterpret_cast<T*>(&rootConstantData[index]);
        *rootConstAddr = value;
    }
};

struct GeometryBuffer
{
    size_t vertexCount = 0;
    size_t maxVertexCount = 65536 * 16;
    size_t vertexStride = 0;
    ID3D12Resource* vertexUploadBuffer = nullptr;
    ID3D12Resource* vertexBuffer = nullptr;
    size_t indexCount = 0;
    size_t maxIndexCount = 65536 * 16;
    size_t indexStride = sizeof(INDEX_BUFFER_FORMAT);
    ID3D12Resource* indexUploadBuffer = nullptr;
    ID3D12Resource* indexBuffer = nullptr;
};

class DebugLineData
{
public:
    DebugLineData(MemoryArena& arena) : lineVertices(arena, MAX_DEBUG_LINE_VERTICES) {}

    FixedList<VertexData::Vertex> lineVertices;
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

inline std::string FormatPlaneOutput(XMVECTOR plane)
{
    float zDist = -XMVectorGetW(plane) / XMVectorGetZ(plane);
    return std::format("Normal: ({},{},{}), ZDist: {}\n",
        plane.m128_f32[0], plane.m128_f32[1], plane.m128_f32[2], zDist);
}

inline float Sgn(float val)
{
    if (val < 0.f) return -1.f;
    if (val > 0.f) return 1.f;
    return 0.f;
}

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

    // Used for portals (or potentially mirrors, water reflections, etc), sets a near clipping plane that isn't parallel to the camera.
    // see: http://www.terathon.com/lengyel/Lengyel-Oblique.pdf
    void UpdateObliqueProjectionMatrix(XMVECTOR nearPlaneNormalWorld, XMVECTOR nearPlanePointWorld)
    {
        float SinFov;
        float CosFov;
        XMScalarSinCos(&SinFov, &CosFov, 0.5f * fovY);

        float Height = CosFov / SinFov;
        float Width = Height / aspectRatio;
        float fRange = farClip / (farClip - nearClip);

        MAT_RMAJ M;
        M.r[0].m128_f32[0] = Width;
        M.r[0].m128_f32[1] = 0.0f;
        M.r[0].m128_f32[2] = 0.0f;
        M.r[0].m128_f32[3] = 0.0f;

        M.r[1].m128_f32[0] = 0.0f;
        M.r[1].m128_f32[1] = Height;
        M.r[1].m128_f32[2] = 0.0f;
        M.r[1].m128_f32[3] = 0.0f;

        M.r[2].m128_f32[0] = 0.0f;
        M.r[2].m128_f32[1] = 0.0f;
        M.r[2].m128_f32[2] = fRange;
        M.r[2].m128_f32[3] = 1.0f;

        M.r[3].m128_f32[0] = 0.0f;
        M.r[3].m128_f32[1] = 0.0f;
        M.r[3].m128_f32[2] = -fRange * nearClip;
        M.r[3].m128_f32[3] = 0.0f;
        
        XMVECTOR clipNormalCamera = XMVector3Normalize(XMVector3TransformNormal(XMVector3Normalize(nearPlaneNormalWorld), worldMatrix.inverse));
        XMVECTOR clipPositionCamera = XMVector3TransformCoord(nearPlanePointWorld, worldMatrix.inverse);
        XMVECTOR clipPlane = PlaneNormalForm(clipNormalCamera, clipPositionCamera);

        XMVECTOR qPrime = {
            Sgn(XMVectorGetX(clipPlane)),
            Sgn(XMVectorGetY(clipPlane)),
            1.f,
            1.f,
        };

        XMVECTOR q = XMVector4Transform(qPrime, XMMatrixInverse(nullptr, M));
        float a = XMVectorGetZ(q) / XMVectorGetX(XMVector4Dot(clipPlane, q));
        XMVECTOR c = XMVectorScale(clipPlane, a);

        M.r[0].m128_f32[2] = XMVectorGetX(c);
        M.r[1].m128_f32[2] = XMVectorGetY(c);
        M.r[2].m128_f32[2] = XMVectorGetZ(c);
        M.r[3].m128_f32[2] = XMVectorGetW(c);

        constantBuffer.data.cameraProjection = XMMatrixTranspose(M);
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

struct GBuffer
{
    constexpr static size_t BUFFER_COUNT = 2;

    UINT width;
    UINT height;
    DXGI_FORMAT format;
    CD3DX12_VIEWPORT viewport;
    CD3DX12_RECT scissorRect;

    Texture textures[BUFFER_COUNT];
    ID3D12Resource* dsvBuffer;

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandles[BUFFER_COUNT];
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle;
};

// TODO: move this somewhere else
struct RTViewport
{
    float left;
    float top;
    float right;
    float bottom;
};

struct RayGenConstantBuffer
{
    RTViewport viewport;
    RTViewport stencil;
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
    TypedMemoryArena<EntityData> entityDataArena = {};

    // Window Handle
    HWND m_hwnd;
    std::wstring m_windowName = L"DirectEngine";

    // Pipeline objects
    CD3DX12_VIEWPORT m_viewport;
    CD3DX12_RECT m_scissorRect;

    ID3D12Device5* m_device = nullptr;
    ID3D12CommandQueue* m_commandQueue = nullptr;
    IDXGISwapChain3* m_swapChain = nullptr;
    ID3D12DescriptorHeap* m_rtvHeap = nullptr;
    ID3D12DescriptorHeap* m_cbvHeap = nullptr;
    ID3D12DescriptorHeap* m_depthStencilHeap = nullptr;
    ID3D12CommandAllocator* m_uploadCommandAllocators[FrameCount] = {};
    ID3D12CommandAllocator* m_renderCommandAllocators[FrameCount] = {};
    ID3D12CommandAllocator* m_raytracingCommandAllocators[FrameCount] = {};
    ID3D12Resource* m_renderTargets[FrameCount] = {};
    ID3D12Resource* m_msaaRenderTargets[FrameCount] = {};
    ID3D12Resource* m_depthStencilBuffer = nullptr;
    ID3D12Resource* m_msaaDepthStencilBuffer = nullptr;
    ID3D12GraphicsCommandList* m_uploadCommandList = nullptr;
    ID3D12GraphicsCommandList4* m_renderCommandList = nullptr;
    ID3D12GraphicsCommandList4* m_raytracingCommandList = nullptr;
    bool m_scheduleUpload = false;
    bool m_renderTextureEnabled = true;
    bool m_msaaEnabled = true;
    UINT m_msaaSampleCount = 4;
    bool m_raytracingEnabled = true;
    PipelineConfig* m_gBufferConfig;
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
    Texture* m_irradianceMap = nullptr;
    GBuffer* m_gBuffer = nullptr;
    ID3D12Resource* m_textureUploadHeaps[MAX_TEXTURES] = {};
    size_t m_textureUploadIndex = 0;
    GeometryBuffer m_geometryBuffer = {};
    FixedList<MaterialData> m_materials = { engineArena, MAX_MATERIALS };
    FixedList<Texture> m_textures = { engineArena, MAX_TEXTURES };
    FixedList<CameraData> m_cameras = { engineArena, MAX_CAMERAS };
    FixedList<MeshData> m_meshes = { engineArena, MAX_MESHES };

    ImGuiUI m_imgui = {};
    CameraData* mainCamera = nullptr;
    FixedList<RenderTexture*> m_renderTextures{ engineArena, 2 };

    // Ray tracing
    ID3D12RootSignature* m_raytracingGlobalRootSignature = nullptr;
    ID3D12RootSignature* m_raytracingLocalRootSignature = nullptr;
    Texture* m_raytracingOutput = nullptr;
    ID3D12StateObject* m_raytracingState = nullptr;
    ID3D12Resource* m_topLevelAccelerationStructure = nullptr;
    ID3D12Resource* m_topLevelScratchResource = nullptr;
    ID3D12Resource* m_topLevelInstanceDescs = nullptr;
    ComPtr<ID3D12Resource> m_missShaderTable = nullptr;
    ComPtr<ID3D12Resource> m_hitGroupShaderTable = nullptr;
    ComPtr<ID3D12Resource> m_rayGenShaderTable = nullptr;
    RayGenConstantBuffer m_rayGenConstantBuffer = {};

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
    bool m_raytracingSupport = false;
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

    const float m_renderTargetClearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    
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
    void CreateUploadBuffer(size_t bufferSizeBytes, ID3D12Resource** resource, wchar_t* name = L"Upload Buffer");
    void CreateGPUBuffer(size_t bufferSizeBytes, ID3D12Resource** resource, wchar_t* name = L"Upload Buffer");
    void LoadAssets();
    void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ID3D12RootSignature** rootSig);
    void LoadRaytracingShaderTables(ID3D12StateObject* dxrStateObject, const wchar_t* raygenShaderName, const wchar_t* missShaderName, const wchar_t* hitGroupShaderName);
    void ResetLevelData();
    CameraData* CreateCamera();
    CD3DX12_CPU_DESCRIPTOR_HANDLE CreateDepthStencilView(UINT width, UINT height, ComStack& comStack, ID3D12Resource** bufferTarget, int fixedOffset = -1, UINT sampleCount = 1);
    void CreatePipeline(PipelineConfig* config, size_t constantBufferCount, size_t rootConstantCount);
    void CreatePipelineState(PipelineConfig* config, bool hotloadShaders = false);
    void CreateRenderTexture(UINT width, UINT height, bool msaaEnabled, RenderTexture& renderTexture, CameraData* camera = nullptr, DXGI_FORMAT textureFormat = DISPLAY_FORMAT);
    void CreateGBuffer(UINT width, UINT height, GBuffer& gBuffer, DXGI_FORMAT textureFormat);
    void CreateEmptyTexture(int width, int height, std::wstring name, Texture& texture, const IID& riidTexture, void** ppvTexture, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    Texture* CreateTexture(const std::wstring& filePath);
    void UploadTexture(const TextureData& textureData, std::vector<D3D12_SUBRESOURCE_DATA>& subresources, Texture& targetTexture);
    size_t CreateMaterial(const std::wstring& shaderName, const std::vector<Texture*>& textures = {}, const std::vector<RootConstantInfo>& rootConstants = {}, const D3D12_RASTERIZER_DESC& rasterizerDesc = CD3DX12_RASTERIZER_DESC{ D3D12_DEFAULT });
    MeshData* CreateMesh(VertexData::MeshFile& meshFile);
    size_t CreateEntity(const size_t materialIndex, MeshData* meshData);
    void BuildBottomLevelAccelerationStructures(ID3D12GraphicsCommandList4* commandList);
    void BuildTopLevelAccelerationStructure(ID3D12GraphicsCommandList4* commandList);
    void ResetVertexBuffer();
    void UploadVertices();
    void RunComputeShaderPrePass(ID3D12GraphicsCommandList4* renderList);
    void RenderGBuffer(ID3D12GraphicsCommandList4* renderList);
    void RaytraceShadows(ID3D12GraphicsCommandList4* renderList);
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