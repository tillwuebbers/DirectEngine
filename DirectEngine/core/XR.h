#pragma once

#ifdef START_WITH_XR

#include "Common.h"

#include <d3d12.h>
#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"

#include <DirectXMath.h>
using namespace DirectX;

#include <wrl.h>
using namespace Microsoft::WRL;
#include <vector>
#include <map>

#define VIEW_CONFIG_TYPE XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO
#define ENVIRONMENT_BLEND_MODE XR_ENVIRONMENT_BLEND_MODE_OPAQUE

struct ViewProjectionConstantBuffer
{
    DirectX::XMFLOAT4X4 ViewProjection;
};

class SwapchainImageContext
{
public:
    void Create(ID3D12Device* d3d12Device, uint32_t capacity);

    uint32_t ImageIndex(const XrSwapchainImageBaseHeader* swapchainImageHeader);

    ID3D12Resource* GetDepthStencilTexture(ID3D12Resource* colorTexture);

    ID3D12CommandAllocator* GetCommandAllocator() const;

    uint64_t GetFrameFenceValue() const;
    void SetFrameFenceValue(uint64_t fenceValue);

    void ResetCommandAllocator();

    ID3D12Device* m_d3d12Device{ nullptr };

    std::vector<XrSwapchainImageD3D12KHR> m_swapchainImages = {};
    ComPtr<ID3D12CommandAllocator> m_commandAllocator = nullptr;
    ComPtr<ID3D12Resource> m_depthStencilTexture = nullptr;
    uint64_t m_fenceValue = 0;

    XrSwapchain handle = nullptr;
    int32_t width = 0;
    int32_t height = 0;
};

struct SwapchainResult
{
    SwapchainImageContext* context;
    uint32_t imageIndex;
    XMMATRIX viewMatrix;
    XMMATRIX projectionMatrix;
};

struct EngineXRState
{
    XrInstance m_instance = { XR_NULL_HANDLE };
    XrSession m_session = { XR_NULL_HANDLE };
    XrSessionState m_sessionState = XrSessionState::XR_SESSION_STATE_UNKNOWN;
    bool m_sessionRunning = false;
    XrSystemId m_systemId = XR_NULL_SYSTEM_ID;
    XrGraphicsBindingD3D12KHR m_graphicsBinding = { XR_TYPE_GRAPHICS_BINDING_D3D12_KHR };
    std::vector<XrSpace> m_visualizedSpaces = {};
    XrSpace m_appSpace = {};
    XrFrameState m_frameState = { XR_TYPE_FRAME_STATE };
    uint32_t m_viewCount = 0;

    HANDLE m_xrFenceEvent = nullptr;
    ComPtr<ID3D12Fence> m_xrFence = nullptr;
    UINT64 m_xrFenceValue = 0;

    XrCompositionLayerProjection m_layer{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    std::vector<XrCompositionLayerBaseHeader*> m_layers = {};
    std::vector<XrCompositionLayerProjectionView> m_projectionLayerViews = {};

    std::vector<XrViewConfigurationView> m_configViews = {};
    std::vector<SwapchainImageContext> m_swapchains = {};
    std::vector<XrView> m_views = {};
    XrEventDataBuffer m_eventDataBuffer;

    Texture m_previewTexture[3];
    uint32_t m_previewWidth = 0;
    uint32_t m_previewHeight = 0;

	LUID InitXR();
	void StartXRSession(ID3D12Device* device, ID3D12CommandQueue* queue);
    const XrEventDataBaseHeader* TryReadNextEvent();
    void PollEvents(bool* exitRenderLoop, bool* requestRestart);
    void HandleSessionStateChangedEvent(const XrEventDataSessionStateChanged& stateChangedEvent, bool* exitRenderLoop, bool* requestRestart);
    void CpuWaitForFence(uint64_t fenceValue);
    void WaitForFrame();
    bool BeginFrame();
    SwapchainResult GetSwapchain(int index);
    void ReleaseSwapchain(int index, SwapchainImageContext* context);
    void EndFrame();
    void ShutdownXR();
};

inline XMVECTOR XrVec3ToXM(XrVector3f vec)
{
    return { vec.x, vec.y, vec.z };
}

inline XMVECTOR XrVec4ToXM(XrVector4f vec)
{
    return std::bit_cast<XMVECTOR>(vec);
}

inline XMVECTOR XrQuatToXM(XrQuaternionf quat)
{
    return std::bit_cast<XMVECTOR>(quat);
}

#endif