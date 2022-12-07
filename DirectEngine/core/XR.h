#pragma once

#include "ComStack.h"

#include <d3d12.h>
#include "openxr/openxr.h"
#include "openxr/openxr_platform.h"

#include <DirectXMath.h>
using namespace DirectX;
#include <vector>
#include <map>

#define VIEW_CONFIG_TYPE XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO
#define ENVIRONMENT_BLEND_MODE XR_ENVIRONMENT_BLEND_MODE_OPAQUE

struct Swapchain {
    XrSwapchain handle;
    int32_t width;
    int32_t height;
};

struct ViewProjectionConstantBuffer {
    DirectX::XMFLOAT4X4 ViewProjection;
};

class SwapchainImageContext {
public:
    std::vector<XrSwapchainImageBaseHeader*> Create(ID3D12Device* d3d12Device, uint32_t capacity, ComStack& comStack);

    uint32_t ImageIndex(const XrSwapchainImageBaseHeader* swapchainImageHeader);

    ID3D12Resource* GetDepthStencilTexture(ID3D12Resource* colorTexture, ComStack& comStack);

    ID3D12CommandAllocator* GetCommandAllocator() const;

    uint64_t GetFrameFenceValue() const;
    void SetFrameFenceValue(uint64_t fenceValue);

    void ResetCommandAllocator();

    void RequestModelCBuffer(uint32_t requiredSize, ComStack& comStack);

    ID3D12Resource* GetModelCBuffer() const;
    ID3D12Resource* GetViewProjectionCBuffer() const;

private:
    ID3D12Device* m_d3d12Device{ nullptr };

    std::vector<XrSwapchainImageD3D12KHR> m_swapchainImages;
    ID3D12CommandAllocator* m_commandAllocator;
    ID3D12Resource* m_depthStencilTexture;
    ID3D12Resource* m_modelCBuffer;
    ID3D12Resource* m_viewProjectionCBuffer;
    uint64_t m_fenceValue = 0;
};

struct SwapchainResult
{
    const XrSwapchainImageD3D12KHR* swapchain;
    SwapchainImageContext* context;
    uint32_t swapchainImageIndex;
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
    ID3D12Fence* m_xrFence = nullptr;
    UINT64 m_xrFenceValue = 0;

    XrCompositionLayerProjection m_layer{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    std::vector<XrCompositionLayerBaseHeader*> m_layers = {};
    std::vector<XrCompositionLayerProjectionView> m_projectionLayerViews = {};

    std::vector<XrViewConfigurationView> m_configViews;
    std::vector<Swapchain> m_swapchains;
    std::map<XrSwapchain, std::vector<XrSwapchainImageBaseHeader*>> m_swapchainImages;
    std::vector<XrView> m_views;
    std::vector<SwapchainImageContext> m_swapchainImageContexts;
    std::map<const XrSwapchainImageBaseHeader*, SwapchainImageContext*> m_swapchainImageContextMap;
    XrEventDataBuffer m_eventDataBuffer;

	LUID InitXR(ComStack& comStack);
	void StartXRSession(ID3D12Device* device, ID3D12CommandQueue* queue, ComStack& comStack);
    const XrEventDataBaseHeader* TryReadNextEvent();
    void PollEvents(bool* exitRenderLoop, bool* requestRestart);
    void HandleSessionStateChangedEvent(const XrEventDataSessionStateChanged& stateChangedEvent, bool* exitRenderLoop, bool* requestRestart);
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
