#include "XR.h"

#include <vector>
#include <format>
#include <assert.h>

#include "../Helpers.h"

void CreateBuffer(ID3D12Device* d3d12Device, uint32_t size, D3D12_HEAP_TYPE heapType, ID3D12Resource** buffer, ComStack& comStack)
{
    D3D12_RESOURCE_STATES d3d12ResourceState;
    if (heapType == D3D12_HEAP_TYPE_UPLOAD)
    {
        d3d12ResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
        size = AlignTo<D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT>(size);
    }
    else
    {
        d3d12ResourceState = D3D12_RESOURCE_STATE_COMMON;
    }

    D3D12_HEAP_PROPERTIES heapProp{};
    heapProp.Type = heapType;
    heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

    D3D12_RESOURCE_DESC buffDesc{};
    buffDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffDesc.Alignment = 0;
    buffDesc.Width = size;
    buffDesc.Height = 1;
    buffDesc.DepthOrArraySize = 1;
    buffDesc.MipLevels = 1;
    buffDesc.Format = DXGI_FORMAT_UNKNOWN;
    buffDesc.SampleDesc.Count = 1;
    buffDesc.SampleDesc.Quality = 0;
    buffDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    buffDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    CHECK_HRCMD(d3d12Device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &buffDesc, d3d12ResourceState, nullptr, NewComObject(comStack, buffer)));
}

std::vector<XrSwapchainImageBaseHeader*> SwapchainImageContext::Create(ID3D12Device* d3d12Device, uint32_t capacity, ComStack& comStack)
{
    m_d3d12Device = d3d12Device;

    m_swapchainImages.resize(capacity);
    std::vector<XrSwapchainImageBaseHeader*> bases(capacity);
    for (uint32_t i = 0; i < capacity; ++i)
    {
        m_swapchainImages[i] = { XR_TYPE_SWAPCHAIN_IMAGE_D3D12_KHR };
        bases[i] = reinterpret_cast<XrSwapchainImageBaseHeader*>(&m_swapchainImages[i]);
    }

    CHECK_HRCMD(m_d3d12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, NewComObject(comStack, &m_commandAllocator)));

    CreateBuffer(m_d3d12Device, sizeof(ViewProjectionConstantBuffer), D3D12_HEAP_TYPE_UPLOAD, &m_viewProjectionCBuffer, comStack);

    return bases;
}

uint32_t SwapchainImageContext::ImageIndex(const XrSwapchainImageBaseHeader* swapchainImageHeader)
{
    auto p = reinterpret_cast<const XrSwapchainImageD3D12KHR*>(swapchainImageHeader);
    return (uint32_t)(p - &m_swapchainImages[0]);
}

ID3D12Resource* SwapchainImageContext::GetDepthStencilTexture(ID3D12Resource* colorTexture, ComStack& comStack)
{
    if (!m_depthStencilTexture) {
        // This back-buffer has no corresponding depth-stencil texture, so create one with matching dimensions.

        const D3D12_RESOURCE_DESC colorDesc = colorTexture->GetDesc();

        D3D12_HEAP_PROPERTIES heapProp{};
        heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_RESOURCE_DESC depthDesc{};
        depthDesc.Dimension = colorDesc.Dimension;
        depthDesc.Alignment = colorDesc.Alignment;
        depthDesc.Width = colorDesc.Width;
        depthDesc.Height = colorDesc.Height;
        depthDesc.DepthOrArraySize = colorDesc.DepthOrArraySize;
        depthDesc.MipLevels = 1;
        depthDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Layout = colorDesc.Layout;
        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.Format = DXGI_FORMAT_D32_FLOAT;

        CHECK_HRCMD(m_d3d12Device->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &depthDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, NewComObject(comStack, &m_depthStencilTexture)));
    }
    return m_depthStencilTexture;
}

ID3D12CommandAllocator* SwapchainImageContext::GetCommandAllocator() const { return m_commandAllocator; }

uint64_t SwapchainImageContext::GetFrameFenceValue() const { return m_fenceValue; }
void SwapchainImageContext::SetFrameFenceValue(uint64_t fenceValue) { m_fenceValue = fenceValue; }

void SwapchainImageContext::ResetCommandAllocator() { CHECK_HRCMD(m_commandAllocator->Reset()); }

void SwapchainImageContext::RequestModelCBuffer(uint32_t requiredSize, ComStack& comStack) {
    if (!m_modelCBuffer || (requiredSize > m_modelCBuffer->GetDesc().Width)) {
        CreateBuffer(m_d3d12Device, requiredSize, D3D12_HEAP_TYPE_UPLOAD, &m_modelCBuffer, comStack);
    }
}

ID3D12Resource* SwapchainImageContext::GetModelCBuffer() const { return m_modelCBuffer; }
ID3D12Resource* SwapchainImageContext::GetViewProjectionCBuffer() const { return m_viewProjectionCBuffer; }


inline std::string GetXrVersionString(XrVersion ver)
{
    return std::format("{}.{}.{}", XR_VERSION_MAJOR(ver), XR_VERSION_MINOR(ver), XR_VERSION_PATCH(ver));
}

void LogLayers()
{
    // Write out extension properties for a given layer.
    const auto logExtensions = [](const char* layerName, int indent = 0) {
        uint32_t instanceExtensionCount;
        ThrowIfFailed(xrEnumerateInstanceExtensionProperties(layerName, 0, &instanceExtensionCount, nullptr));
        std::vector<XrExtensionProperties> extensions(instanceExtensionCount, { XR_TYPE_EXTENSION_PROPERTIES });
        ThrowIfFailed(xrEnumerateInstanceExtensionProperties(layerName, (uint32_t)extensions.size(), &instanceExtensionCount,
            extensions.data()));

        OutputDebugString(std::format(L"Available Extensions: ({})\n", instanceExtensionCount).c_str());
        for (const XrExtensionProperties& extension : extensions)
        {
            OutputDebugStringA(std::format("  SpecVersion {}: {}\n", extension.extensionVersion, extension.extensionName).c_str());
        }
    };

    // Log non-layer extensions (layerName==nullptr).
    logExtensions(nullptr);

    // Log layers and any of their extensions.
    {
        uint32_t layerCount;
        ThrowIfFailed(xrEnumerateApiLayerProperties(0, &layerCount, nullptr));
        std::vector<XrApiLayerProperties> layers(layerCount, { XR_TYPE_API_LAYER_PROPERTIES });
        ThrowIfFailed(xrEnumerateApiLayerProperties((uint32_t)layers.size(), &layerCount, layers.data()));

        OutputDebugString(std::format(L"Available Layers: ({})\n", layerCount).c_str());
        for (const XrApiLayerProperties& layer : layers) {
            OutputDebugStringA(std::format("  Name={} SpecVersion={} LayerVersion={} Description={}\n", layer.layerName, GetXrVersionString(layer.specVersion).c_str(), layer.layerVersion, layer.description).c_str());
            logExtensions(layer.layerName, 4);
        }
    }
}

LUID EngineXRState::InitXR(ComStack& comStack)
{
    LogLayers();

    const char* extensions[] = { XR_KHR_D3D12_ENABLE_EXTENSION_NAME };

    XrInstanceCreateInfo instanceCreate{ XR_TYPE_INSTANCE_CREATE_INFO };
    instanceCreate.next = nullptr;
    instanceCreate.enabledExtensionCount = static_cast<uint32_t>(_countof(extensions));
    instanceCreate.enabledExtensionNames = extensions;
    strcpy_s(instanceCreate.applicationInfo.applicationName, "DirectEngine");
    instanceCreate.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    ThrowIfFailed(xrCreateInstance(&instanceCreate, &m_instance));
    XrSystemGetInfo systemInfo{ XR_TYPE_SYSTEM_GET_INFO };
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    CHECK_HRCMD(xrGetSystem(m_instance, &systemInfo, &m_systemId));

    PFN_xrGetD3D12GraphicsRequirementsKHR pfnGetD3D12GraphicsRequirementsKHR = nullptr;
    CHECK_HRCMD(xrGetInstanceProcAddr(m_instance, "xrGetD3D12GraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetD3D12GraphicsRequirementsKHR)));

    XrGraphicsRequirementsD3D12KHR graphicsRequirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_D3D12_KHR };
    CHECK_HRCMD(pfnGetD3D12GraphicsRequirementsKHR(m_instance, m_systemId, &graphicsRequirements));

    return graphicsRequirements.adapterLuid;
}

void EngineXRState::StartXRSession(ID3D12Device* device, ID3D12CommandQueue* queue, ComStack& comStack)
{
    m_graphicsBinding.device = device;
    m_graphicsBinding.queue = queue;

    // Session
    XrSessionCreateInfo sessionCreate{ XR_TYPE_SESSION_CREATE_INFO };
    sessionCreate.next = reinterpret_cast<const XrBaseInStructure*>(&m_graphicsBinding);
    sessionCreate.systemId = m_systemId;
    CHECK_HRCMD(xrCreateSession(m_instance, &sessionCreate, &m_session));

    // All Spaces
    XrReferenceSpaceType visualizedSpaces[] = { XR_REFERENCE_SPACE_TYPE_VIEW, XR_REFERENCE_SPACE_TYPE_STAGE, XR_REFERENCE_SPACE_TYPE_LOCAL };
    for (const auto& visualizedSpace : visualizedSpaces)
    {
        XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
        referenceSpaceCreateInfo.poseInReferenceSpace = {};
        referenceSpaceCreateInfo.poseInReferenceSpace.orientation.w = 1.f;
        referenceSpaceCreateInfo.referenceSpaceType = visualizedSpace;
        XrSpace space;
        CHECK_HRCMD(xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &space));
        m_visualizedSpaces.push_back(space);
    }

    // App space
    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    referenceSpaceCreateInfo.poseInReferenceSpace = {};
    referenceSpaceCreateInfo.poseInReferenceSpace.orientation.w = 1.f;
    referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    CHECK_HRCMD(xrCreateReferenceSpace(m_session, &referenceSpaceCreateInfo, &m_appSpace));

    XrSystemProperties systemProperties{ XR_TYPE_SYSTEM_PROPERTIES };
    CHECK_HRCMD(xrGetSystemProperties(m_instance, m_systemId, &systemProperties));

    // Query and cache view configuration views.
    uint32_t viewCount;
    CHECK_HRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, VIEW_CONFIG_TYPE, 0, &viewCount, nullptr));
    m_configViews.resize(viewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
    CHECK_HRCMD(xrEnumerateViewConfigurationViews(m_instance, m_systemId, VIEW_CONFIG_TYPE, viewCount, &viewCount, m_configViews.data()));

    // Create and cache view buffer for xrLocateViews later.
    m_views.resize(viewCount, { XR_TYPE_VIEW });

    // Create the swapchain and get the images.
    if (viewCount > 0)
    {
        // Select a swapchain format.
        uint32_t swapchainFormatCount;
        CHECK_HRCMD(xrEnumerateSwapchainFormats(m_session, 0, &swapchainFormatCount, nullptr));
        std::vector<int64_t> swapchainFormats(swapchainFormatCount);
        CHECK_HRCMD(xrEnumerateSwapchainFormats(m_session, (uint32_t)swapchainFormats.size(), &swapchainFormatCount,
            swapchainFormats.data()));
        assert(swapchainFormatCount == swapchainFormats.size());

        uint64_t m_colorSwapchainFormat = swapchainFormats.at(0);
        // Print swapchain formats and the selected one.
        {
            std::string swapchainFormatsString;
            for (int64_t format : swapchainFormats) {
                const bool selected = format == m_colorSwapchainFormat;
                swapchainFormatsString += " ";
                if (selected) {
                    swapchainFormatsString += "[";
                }
                swapchainFormatsString += std::to_string(format);
                if (selected) {
                    swapchainFormatsString += "]";
                }
            }
            OutputDebugStringA(std::format("Swapchain Formats: {}\n", swapchainFormatsString).c_str());
        }

        // Create a swapchain for each view.
        m_swapchainImageContexts.resize(viewCount);
        for (uint32_t i = 0; i < viewCount; i++)
        {
            const XrViewConfigurationView& vp = m_configViews[i];
            OutputDebugString(std::format(L"Creating swapchain for view {} with dimensions Width={} Height={} SampleCount={}\n", i, vp.recommendedImageRectWidth, vp.recommendedImageRectHeight, vp.recommendedSwapchainSampleCount).c_str());

            // Create the swapchain.
            XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
            swapchainCreateInfo.arraySize = 1;
            swapchainCreateInfo.format = m_colorSwapchainFormat;
            swapchainCreateInfo.width = vp.recommendedImageRectWidth;
            swapchainCreateInfo.height = vp.recommendedImageRectHeight;
            swapchainCreateInfo.mipCount = 1;
            swapchainCreateInfo.faceCount = 1;
            swapchainCreateInfo.sampleCount = vp.recommendedSwapchainSampleCount;
            swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            Swapchain swapchain;
            swapchain.width = swapchainCreateInfo.width;
            swapchain.height = swapchainCreateInfo.height;
            CHECK_HRCMD(xrCreateSwapchain(m_session, &swapchainCreateInfo, &swapchain.handle));

            m_swapchains.push_back(swapchain);

            uint32_t imageCount;
            CHECK_HRCMD(xrEnumerateSwapchainImages(swapchain.handle, 0, &imageCount, nullptr));

            // Allocate and initialize the buffer of image structs (must be sequential in memory for xrEnumerateSwapchainImages).
            // Return back an array of pointers to each swapchain image struct so the consumer doesn't need to know the type/size.
            SwapchainImageContext& swapchainImageContext = m_swapchainImageContexts[i];
            std::vector<XrSwapchainImageBaseHeader*> swapchainImages = swapchainImageContext.Create(device, imageCount, comStack);

            // Map every swapchainImage base pointer to this context
            for (auto& base : swapchainImages)
            {
                m_swapchainImageContextMap[base] = &swapchainImageContext;
            }

            CHECK_HRCMD(xrEnumerateSwapchainImages(swapchain.handle, imageCount, &imageCount, swapchainImages[0]));

            m_swapchainImages.insert(std::make_pair(swapchain.handle, std::move(swapchainImages)));
        }
    }
}

const XrEventDataBaseHeader* EngineXRState::TryReadNextEvent() {
    // It is sufficient to clear the just the XrEventDataBuffer header to
    // XR_TYPE_EVENT_DATA_BUFFER
    XrEventDataBaseHeader* baseHeader = reinterpret_cast<XrEventDataBaseHeader*>(&m_eventDataBuffer);
    *baseHeader = { XR_TYPE_EVENT_DATA_BUFFER };
    const XrResult xr = xrPollEvent(m_instance, &m_eventDataBuffer);
    if (xr == XR_SUCCESS) {
        if (baseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
            const XrEventDataEventsLost* const eventsLost = reinterpret_cast<const XrEventDataEventsLost*>(baseHeader);
            //Log::Write(Log::Level::Warning, Fmt("%d events lost", eventsLost));
        }

        return baseHeader;
    }
    if (xr == XR_EVENT_UNAVAILABLE) {
        return nullptr;
    }
    //THROW_XR(xr, "xrPollEvent");
}

void EngineXRState::PollEvents(bool* exitRenderLoop, bool* requestRestart) {
    *exitRenderLoop = *requestRestart = false;

    // Process all pending messages.
    while (const XrEventDataBaseHeader* event = TryReadNextEvent()) {
        switch (event->type) {
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: {
            const auto& instanceLossPending = *reinterpret_cast<const XrEventDataInstanceLossPending*>(event);
            //Log::Write(Log::Level::Warning, Fmt("XrEventDataInstanceLossPending by %lld", instanceLossPending.lossTime));
            *exitRenderLoop = true;
            *requestRestart = true;
            return;
        }
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            auto sessionStateChangedEvent = *reinterpret_cast<const XrEventDataSessionStateChanged*>(event);
            HandleSessionStateChangedEvent(sessionStateChangedEvent, exitRenderLoop, requestRestart);
            break;
        }
        case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
            /*LogActionSourceName(m_input.grabAction, "Grab");
            LogActionSourceName(m_input.quitAction, "Quit");
            LogActionSourceName(m_input.poseAction, "Pose");
            LogActionSourceName(m_input.vibrateAction, "Vibrate");*/
            break;
        case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
        default: {
            //Log::Write(Log::Level::Verbose, Fmt("Ignoring event type %d", event->type));
            break;
        }
        }
    }
}

void EngineXRState::HandleSessionStateChangedEvent(const XrEventDataSessionStateChanged& stateChangedEvent, bool* exitRenderLoop,
    bool* requestRestart) {
    const XrSessionState oldState = m_sessionState;
    m_sessionState = stateChangedEvent.state;

    //Log::Write(Log::Level::Info, Fmt("XrEventDataSessionStateChanged: state %s->%s session=%lld time=%lld", to_string(oldState), to_string(m_sessionState), stateChangedEvent.session, stateChangedEvent.time));

    if ((stateChangedEvent.session != XR_NULL_HANDLE) && (stateChangedEvent.session != m_session)) {
        //Log::Write(Log::Level::Error, "XrEventDataSessionStateChanged for unknown session");
        return;
    }

    switch (m_sessionState) {
    case XR_SESSION_STATE_READY: {
        assert(m_session != XR_NULL_HANDLE);
        XrSessionBeginInfo sessionBeginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
        sessionBeginInfo.primaryViewConfigurationType = VIEW_CONFIG_TYPE;
        CHECK_HRCMD(xrBeginSession(m_session, &sessionBeginInfo));
        m_sessionRunning = true;
        break;
    }
    case XR_SESSION_STATE_STOPPING: {
        assert(m_session != XR_NULL_HANDLE);
        m_sessionRunning = false;
        CHECK_HRCMD(xrEndSession(m_session))
            break;
    }
    case XR_SESSION_STATE_EXITING: {
        *exitRenderLoop = true;
        // Do not attempt to restart because user closed this session.
        *requestRestart = false;
        break;
    }
    case XR_SESSION_STATE_LOSS_PENDING: {
        *exitRenderLoop = true;
        // Poll for a new instance.
        *requestRestart = true;
        break;
    }
    default:
        break;
    }
}

void EngineXRState::WaitForFrame()
{
    bool quit = false;
    bool restart = false;
    PollEvents(&quit, &restart);
    if (!m_sessionRunning) return;

    XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
    CHECK_HRCMD(xrWaitFrame(m_session, &frameWaitInfo, &m_frameState));
}

bool EngineXRState::BeginFrame()
{
    if (!m_sessionRunning) return false;

    m_layers.clear();
    m_projectionLayerViews.clear();

    XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
    CHECK_HRCMD(xrBeginFrame(m_session, &frameBeginInfo));

    if (m_frameState.shouldRender == XR_TRUE)
    {
        XrViewState viewState{ XR_TYPE_VIEW_STATE };
        uint32_t viewCapacityInput = (uint32_t)m_views.size();

        XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
        viewLocateInfo.viewConfigurationType = VIEW_CONFIG_TYPE;
        viewLocateInfo.displayTime = m_frameState.predictedDisplayTime;
        viewLocateInfo.space = m_appSpace;

        CHECK_HRCMD(xrLocateViews(m_session, &viewLocateInfo, &viewState, viewCapacityInput, &m_viewCount, m_views.data()));
        if ((viewState.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) == 0 ||
            (viewState.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT) == 0) {
            return false;  // There is no valid tracking poses for the views.
        }

        assert(m_viewCount == viewCapacityInput);
        assert(m_viewCount == m_configViews.size());
        assert(m_viewCount == m_swapchains.size());

        m_projectionLayerViews.resize(m_viewCount);
    }

    return true;
}

SwapchainResult EngineXRState::GetSwapchain(int index)
{
    // Each view has a separate swapchain which is acquired, rendered to, and released.
    const Swapchain viewSwapchain = m_swapchains[index];

    XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };

    uint32_t swapchainImageIndex;
    CHECK_HRCMD(xrAcquireSwapchainImage(viewSwapchain.handle, &acquireInfo, &swapchainImageIndex));

    XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = XR_INFINITE_DURATION;
    CHECK_HRCMD(xrWaitSwapchainImage(viewSwapchain.handle, &waitInfo));

    XrCompositionLayerProjectionView& projectionLayerView = m_projectionLayerViews[index];
    projectionLayerView = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
    projectionLayerView.pose = m_views[index].pose;
    projectionLayerView.fov = m_views[index].fov;
    projectionLayerView.subImage.swapchain = viewSwapchain.handle;
    projectionLayerView.subImage.imageRect.offset = { 0, 0 };
    projectionLayerView.subImage.imageRect.extent = { viewSwapchain.width, viewSwapchain.height };

    const XrSwapchainImageBaseHeader* const swapchainImage = m_swapchainImages[viewSwapchain.handle][swapchainImageIndex];

    //outViewMatrix = XMMatrixInverse(nullptr, XMMatrixAffineTransformation({ 1.f, 1.f, 1.f }, {}, XrQuatToXM(projectionLayerView.pose.orientation), XrVec3ToXM(projectionLayerView.pose.position)));

    //XrFovf& fov = projectionLayerView.fov;
    //outProjectionMatrix = XMMatrixPerspectiveOffCenterLH(tanf(fov.angleLeft), tanf(fov.angleRight), tanf(fov.angleDown), tanf(fov.angleUp), 0.05f, 100.f);

    return { reinterpret_cast<const XrSwapchainImageD3D12KHR*>(swapchainImage) };
}

void EngineXRState::ReleaseSwapchain(int index)
{
    XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    CHECK_HRCMD(xrReleaseSwapchainImage(m_swapchains[index].handle, &releaseInfo));
}

void EngineXRState::EndFrame()
{
    if (m_frameState.shouldRender == XR_TRUE)
    {
        m_layer.space = m_appSpace;
        m_layer.layerFlags =
            ENVIRONMENT_BLEND_MODE == XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND
            ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT
            : 0;
        m_layer.viewCount = (uint32_t)m_projectionLayerViews.size();
        m_layer.views = m_projectionLayerViews.data();

        m_layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&m_layer));
    }

    XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
    frameEndInfo.displayTime = m_frameState.predictedDisplayTime;
    frameEndInfo.environmentBlendMode = ENVIRONMENT_BLEND_MODE;
    frameEndInfo.layerCount = (uint32_t)m_layers.size();
    frameEndInfo.layers = m_layers.data();
    CHECK_HRCMD(xrEndFrame(m_session, &frameEndInfo));
}

void EngineXRState::ShutdownXR()
{
    for (Swapchain swapchain : m_swapchains)
    {
        xrDestroySwapchain(swapchain.handle);
    }

    for (XrSpace visualizedSpace : m_visualizedSpaces)
    {
        xrDestroySpace(visualizedSpace);
    }

    if (m_session != XR_NULL_HANDLE)
    {
        xrDestroySession(m_session);
    }

    if (m_instance != XR_NULL_HANDLE)
    {
        xrDestroyInstance(m_instance);
    }
}