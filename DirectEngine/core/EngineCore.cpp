#pragma warning (disable : 4996)

#include <comdef.h>

#include <iostream>
#include <codecvt>
#include <locale>

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <dxgidebug.h>

#if defined(_DEBUG)
#include <fstream>
#endif

#include "UI.h"
#include "EngineCore.h"
#include "../directx-tex/DDSTextureLoader12.h"

EngineCore::EngineCore(UINT width, UINT height, IGame* game) :
    m_frameIndex(0),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_fenceValues{},
    m_rtvDescriptorSize(0),
    m_width(width),
    m_height(height),
    m_aspectRatio((float)width / (float)height),
    m_game(game)
{}

void EngineCore::OnInit(HINSTANCE hInst, int nCmdShow, WNDPROC wndProc)
{
    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

    // window
    CheckTearingSupport();
    const wchar_t* windowClassName = L"DX12WindowClass";
    RegisterWindowClass(hInst, windowClassName, wndProc);
    CreateGameWindow(windowClassName, hInst, m_width, m_height);

    // directx
#ifdef START_WITH_XR
    LUID requiredLuid = m_xrState.InitXR(comPointers);
    LoadPipeline(&requiredLuid);
    m_xrState.StartXRSession(m_device, m_commandQueue, comPointers);
#else
    LoadPipeline(nullptr);
#endif
    LoadAssets();

    // Audio
    ThrowIfFailed(XAudio2Create(&m_audio, 0, XAUDIO2_DEFAULT_PROCESSOR));
    ThrowIfFailed(m_audio->CreateMasteringVoice(&m_audioMasteringVoice));

    DWORD channelMask;
    m_audioMasteringVoice->GetChannelMask(&channelMask);
    X3DAudioInitialize(channelMask, 343., m_3daudio);
    m_audioMasteringVoice->GetVoiceDetails(&m_audioVoiceDetails);

    m_audioDspSettingsMono.SrcChannelCount = 1;
    m_audioDspSettingsMono.DstChannelCount = m_audioVoiceDetails.InputChannels;
    m_audioDspSettingsMono.pMatrixCoefficients = NewArray(engineArena, FLOAT32, m_audioVoiceDetails.InputChannels);

    m_audioDspSettingsStereo.SrcChannelCount = 2;
    m_audioDspSettingsStereo.DstChannelCount = m_audioVoiceDetails.InputChannels;
    m_audioDspSettingsStereo.pMatrixCoefficients = NewArray(engineArena, FLOAT32, m_audioVoiceDetails.InputChannels);

    AudioBuffer* testAudio = LoadAudioFile(L"audio/chord.wav", engineArena);
    m_defaultAudioFormat = (WAVEFORMATEX*)&testAudio->wfx;

    // Time
    m_startTime = std::chrono::high_resolution_clock::now();
    m_frameStartTime = m_startTime;

    ShowWindow(m_hwnd, nCmdShow);
}

void EngineCore::RegisterWindowClass(HINSTANCE hInst, const wchar_t* windowClassName, WNDPROC wndProc)
{
    // Register a window class for creating our render window with.
    WNDCLASSEXW windowClass = {};

    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = wndProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = hInst;
    windowClass.hIcon = ::LoadIcon(hInst, NULL);
    windowClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    windowClass.lpszMenuName = NULL;
    windowClass.lpszClassName = windowClassName;
    windowClass.hIconSm = ::LoadIcon(hInst, NULL);

    static ATOM atom = ::RegisterClassExW(&windowClass);
    assert(atom > 0);
}

void EngineCore::CreateGameWindow(const wchar_t* windowClassName, HINSTANCE hInst, uint32_t width, uint32_t height)
{
    int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

    RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    ::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    int windowWidth = windowRect.right - windowRect.left;
    int windowHeight = windowRect.bottom - windowRect.top;

    // Center the window within the screen. Clamp to 0, 0 for the top-left corner.
    int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
    int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

    m_hwnd = ::CreateWindowExW(
        NULL,
        windowClassName,
        m_windowName.c_str(),
        WS_OVERLAPPEDWINDOW,
        windowX,
        windowY,
        windowWidth,
        windowHeight,
        NULL,
        NULL,
        hInst,
        nullptr
    );

    assert(m_hwnd && "Failed to create window");
}

// Load the rendering pipeline dependencies.
void EngineCore::LoadPipeline(LUID* requiredLuid)
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debugController;
        ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            
            ThrowIfFailed(debugController->QueryInterface(IID_PPV_ARGS(&debugController1)));
            debugController1->SetEnableGPUBasedValidation(true);

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory6> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, NewComObject(comPointers, &m_device)));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;

        for (UINT adapterIndex = 0; SUCCEEDED(factory->EnumAdapterByGpuPreference(adapterIndex, GPU_PREFERENCE, IID_PPV_ARGS(&hardwareAdapter))); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            hardwareAdapter->GetDesc1(&desc);

            // Don't select the Basic Render Driver adapter.
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            // Check LUID
            if (requiredLuid != nullptr && (desc.AdapterLuid.HighPart != requiredLuid->HighPart || desc.AdapterLuid.LowPart != requiredLuid->LowPart))
            {
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }

        if (hardwareAdapter == nullptr)
        {
            for (UINT adapterIndex = 0; SUCCEEDED(factory->EnumAdapters1(adapterIndex, &hardwareAdapter)); ++adapterIndex)
            {
                DXGI_ADAPTER_DESC1 desc;
                hardwareAdapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    continue;
                }

                // Check LUID
                if (requiredLuid != nullptr && (desc.AdapterLuid.HighPart != requiredLuid->HighPart || desc.AdapterLuid.LowPart != requiredLuid->LowPart))
                {
                    continue;
                }

                // Check to see whether the adapter supports Direct3D 12, but don't create the
                // actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        ThrowIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, NewComObject(comPointers, &m_device)));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, NewComObject(comPointers, &m_commandQueue)));
    m_commandQueue->SetName(L"Engine Command Queue");

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Flags = (m_tearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0) | DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    IDXGISwapChain1* swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(m_commandQueue, m_hwnd, &swapChainDesc, nullptr, nullptr, &swapChain));
    ThrowIfFailed(swapChain->QueryInterface(NewComObject(comPointers, &m_swapChain)));
    swapChain->Release();
    
    m_swapChain->SetMaximumFrameLatency(1);

#ifndef START_WITH_XR
    m_frameWaitableObject = m_swapChain->GetFrameLatencyWaitableObject();
#endif

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount * 2; // TODO
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, NewComObject(comPointers, &m_rtvHeap)));
        m_rtvHeap->SetName(L"Render Target View Descriptor Heap");

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_dsvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        // Describe and create a constant buffer view (CBV) descriptor heap.
        // Flags indicate that this descriptor heap can be bound to the pipeline 
        // and that descriptors contained in it can be referenced by a root table.
        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
        cbvHeapDesc.NumDescriptors = MAX_DESCRIPTORS;
        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, NewComObject(comPointers, &m_cbvHeap)));
        m_cbvHeap->SetName(L"Constant Buffer View Descriptor Heap");

        // Depth/Stencil descriptor heap
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = FrameCount * 2; // TODO
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, NewComObject(comPointers, &m_depthStencilHeap)));
        m_depthStencilHeap->SetName(L"Depth/Stencil Descriptor Heap");
    }

    for (UINT n = 0; n < FrameCount; n++)
    {
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, NewComObject(comPointers, &m_uploadCommandAllocators[n])));
        m_uploadCommandAllocators[n]->SetName(std::format(L"Upload Command Allocator {}", n).c_str());
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, NewComObject(comPointers, &m_renderCommandAllocators[n])));
        m_renderCommandAllocators[n]->SetName(std::format(L"Render Command Allocator {}", n).c_str());
    }

    LoadSizeDependentResources();

    SetupImgui(m_hwnd, this, FrameCount);
}

void EngineCore::LoadSizeDependentResources()
{
    // Create frame resources.
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DISPLAY_FORMAT;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            //ThrowIfFailed(m_swapChain->GetBuffer(n, NewComObject(comPointersSizeDependent, &m_renderTargets[n])));
            //m_device->CreateRenderTargetView(m_renderTargets[n], &rtvDesc, rtvHandle);
            //m_renderTargets[n]->SetName(std::format(L"Engine Render Target {}", n).c_str());
            //rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    // depth/stencil view
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
        depthStencilDesc.Format = DEPTH_BUFFER_FORMAT;
        depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DEPTH_BUFFER_FORMAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        CD3DX12_HEAP_PROPERTIES heapType(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC depthTexture = CD3DX12_RESOURCE_DESC::Tex2D(DEPTH_BUFFER_FORMAT_TYPELESS, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        ThrowIfFailed(m_device->CreateCommittedResource(&heapType, D3D12_HEAP_FLAG_NONE, &depthTexture, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue, NewComObject(comPointersSizeDependent, &m_depthStencilBuffer)));
        m_depthStencilBuffer->SetName(L"Depth/Stencil Buffer");

        m_device->CreateDepthStencilView(m_depthStencilBuffer, &depthStencilDesc, m_depthStencilHeap->GetCPUDescriptorHandleForHeapStart());
    }
}

PipelineConfig* EngineCore::CreatePipeline(ShaderDescription shaderDesc, size_t textureCount, size_t constantBufferCount, size_t rootConstantCount, bool wireframe = false, bool ignoreDepth = false)
{
    PipelineConfig* config = NewObject(engineArena, PipelineConfig);
    config->shaderDescription = shaderDesc;
    config->textureSlotCount = textureCount;
    config->wireframe = wireframe;
    config->ignoreDepth = ignoreDepth;
    config->creationError = ERROR_SUCCESS;

    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        std::vector<CD3DX12_DESCRIPTOR_RANGE1> ranges{CUSTOM_START + textureCount + constantBufferCount};
        std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters{CUSTOM_START + textureCount + constantBufferCount + rootConstantCount};

        ranges[SCENE].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, SCENE, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[LIGHT].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, LIGHT, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[ENTITY].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, ENTITY, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[BONES].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, BONES, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[SHADOWMAP].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, SHADOWMAP, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        
        rootParameters[SCENE].InitAsDescriptorTable(1, &ranges[SCENE], D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[LIGHT].InitAsDescriptorTable(1, &ranges[LIGHT], D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[ENTITY].InitAsDescriptorTable(1, &ranges[ENTITY], D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters[BONES].InitAsDescriptorTable(1, &ranges[BONES], D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters[SHADOWMAP].InitAsDescriptorTable(1, &ranges[SHADOWMAP], D3D12_SHADER_VISIBILITY_PIXEL);
        
        for (int i = 0; i < config->textureSlotCount; i++)
        {
            int offset = CUSTOM_START + i;
            ranges[offset].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, offset, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
            rootParameters[offset].InitAsDescriptorTable(1, &ranges[offset], D3D12_SHADER_VISIBILITY_PIXEL);
        }

        for (int i = 0; i < constantBufferCount; i++)
        {
            int offset = CUSTOM_START + config->textureSlotCount + i;
            ranges[offset].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, offset, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
            rootParameters[offset].InitAsDescriptorTable(1, &ranges[offset], D3D12_SHADER_VISIBILITY_ALL);
        }

        for (int i = 0; i < rootConstantCount; i++)
        {
            int offset = CUSTOM_START + config->textureSlotCount + constantBufferCount + i;
            rootParameters[offset].InitAsConstants(1, offset);
        }

        D3D12_STATIC_SAMPLER_DESC rawSampler = {};
        rawSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        rawSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        rawSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        rawSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        rawSampler.MipLODBias = 0;
        rawSampler.MaxAnisotropy = 0;
        rawSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        rawSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        rawSampler.MinLOD = 0.0f;
        rawSampler.MaxLOD = D3D12_FLOAT32_MAX;
        rawSampler.ShaderRegister = 0;
        rawSampler.RegisterSpace = 0;
        rawSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC smoothSampler = {};
        smoothSampler.Filter = D3D12_FILTER_ANISOTROPIC;
        smoothSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        smoothSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        smoothSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        smoothSampler.MipLODBias = 0;
        smoothSampler.MaxAnisotropy = 16;
        smoothSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        smoothSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        smoothSampler.MinLOD = 0.0f;
        smoothSampler.MaxLOD = D3D12_FLOAT32_MAX;
        smoothSampler.ShaderRegister = 1;
        smoothSampler.RegisterSpace = 0;
        smoothSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        std::vector<D3D12_STATIC_SAMPLER_DESC> samplers = { rawSampler, smoothSampler };

        // Allow input layout and deny uneccessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
        rootSignatureDesc.Init_1_1(rootParameters.size(), rootParameters.data(), samplers.size(), samplers.data(), rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        if (FAILED(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error)))
        {
            OutputDebugStringA(reinterpret_cast<const char*>(error->GetBufferPointer()));
        }
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), NewComObject(comPointers, &config->rootSignature)));
        config->rootSignature->SetName(config->shaderDescription.debugName);
    }

    CreatePipelineState(config);
    return config;
}

void EngineCore::CreatePipelineState(PipelineConfig* config)
{
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    std::wstring path = std::wstring(L"../../../../DirectEngine/shaders/");
    path.append(config->shaderDescription.shaderFileName);
    const wchar_t* shaderPath = path.c_str();

    Sleep(100);
    bool canOpen = false;
    while (!canOpen)
    {
        std::ifstream fileStream(shaderPath, std::ios::in);
        canOpen = fileStream.good();
        fileStream.close();

        if (!canOpen)
        {
            Sleep(100);
        }
    }
#else
    UINT compileFlags = 0;
    std::wstring path = std::wstring(L"shaders/");
    path.append(config->shaderDescription.shaderFileName);
    const wchar_t* shaderPath = path.c_str();
#endif

    ComPtr<ID3DBlob> vsErrors;
    ComPtr<ID3D10Blob> psErrors;

    m_shaderError.clear();
    config->creationError = D3DCompileFromFile(shaderPath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, config->shaderDescription.vsEntryName, "vs_5_0", compileFlags, 0, &vertexShader, &vsErrors);
    if (vsErrors)
    {
        m_shaderError = std::format("Vertex Shader Errors:\n{}\n", (LPCSTR)vsErrors->GetBufferPointer());
        OutputDebugString(std::format(L"{}\n", shaderPath).c_str());
        OutputDebugStringA(m_shaderError.c_str());
    }
    if (FAILED(config->creationError)) return;

    config->creationError = D3DCompileFromFile(shaderPath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, config->shaderDescription.psEntryName, "ps_5_0", compileFlags, 0, &pixelShader, &psErrors);
    if (psErrors)
    {
        m_shaderError = std::format("Pixel Shader Errors:\n{}\n", (LPCSTR)psErrors->GetBufferPointer());
        OutputDebugString(std::format(L"{}\n", shaderPath).c_str());
        OutputDebugStringA(m_shaderError.c_str());
    }
    if (FAILED(config->creationError)) return;

    // Define the vertex input layout.
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "UV",           0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONE_WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONE_INDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT,  0, 64, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // Rasterizer
    CD3DX12_RASTERIZER_DESC rasterizerDesc(D3D12_DEFAULT);
    if (config->wireframe)
    {
        rasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    }

    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = config->rootSignature;
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthFunc = config->ignoreDepth ? D3D12_COMPARISON_FUNC_ALWAYS : D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DISPLAY_FORMAT;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.DSVFormat = DEPTH_BUFFER_FORMAT;

    config->creationError = m_device->CreateGraphicsPipelineState(&psoDesc, NewComObjectReplace(comPointers, &config->pipelineState));
    config->pipelineState->SetName(config->shaderDescription.debugName);
}

void EngineCore::LoadAssets()
{
    m_shadowConfig = CreatePipeline({ L"shadow.hlsl", "VSShadow", "PSShadow", L"Shadow" }, 0, 0, 0);
    m_boneDebugConfig = CreatePipeline({ L"bones.hlsl", "VSMain", "PSMain", L"Bones" }, 0, 0, 1, true, true);
    m_wireframeConfig = CreatePipeline({ L"aabb.hlsl", "VSWire", "PSWire", L"Wireframe" }, 0, 0, 0, true);

    // Create the render command list
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_renderCommandAllocators[m_frameIndex], nullptr, NewComObject(comPointers, &m_renderCommandList)));
    ThrowIfFailed(m_renderCommandList->Close());
    m_renderCommandList->SetName(L"Render Command List");

    // Create the upload command list
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_uploadCommandAllocators[m_frameIndex], nullptr, NewComObject(comPointers, &m_uploadCommandList)));
    m_uploadCommandList->SetName(L"Upload Command List");

    // Shader values for scene
    CreateConstantBuffers<SceneConstantBuffer>(m_sceneConstantBuffer, L"Scene Constant Buffer");
    CreateConstantBuffers<LightConstantBuffer>(m_lightConstantBuffer, L"Light Constant Buffer");
    for (int i = 0; i < FrameCount; i++)
    {
        m_sceneConstantBuffer.UploadData(i);
        m_lightConstantBuffer.UploadData(i);
    }

    m_shadowmap = NewObject(engineArena, ShadowMap, DEPTH_BUFFER_FORMAT_TYPELESS);

    m_shadowmap->depthStencilViewCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_depthStencilHeap->GetCPUDescriptorHandleForHeapStart(), 1, m_dsvDescriptorSize);
    m_shadowmap->shaderResourceViewHandle = GetNewDescriptorHandle();
    m_shadowmap->SetSize(SHADOWMAP_SIZE, SHADOWMAP_SIZE);
    m_shadowmap->Build(m_device, comPointers);

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, NewComObject(comPointers, &m_fence)));
        m_fence->SetName(L"Engine Fence");
        m_fenceValues[m_frameIndex]++;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForGpu();
    }
}

void EngineCore::CreateTexture(Texture& outTexture, const wchar_t* filePath)
{
    TextureData header = ParseDDSHeader(filePath);
    std::unique_ptr<uint8_t[]> data{};
    std::vector<D3D12_SUBRESOURCE_DATA> subresources{};
    LoadDDSTextureFromFile(m_device, filePath, &outTexture.buffer, data, subresources);
    comPointers.AddPointer((void**)&outTexture.buffer);
    UploadTexture(header, subresources, outTexture);
}

void EngineCore::UploadTexture(const TextureData& textureData, std::vector<D3D12_SUBRESOURCE_DATA>& subresources, Texture& targetTexture)
{
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.MipLevels = textureData.mipmapCount;
    textureDesc.Format = textureData.format;
    textureDesc.Width = textureData.width;
    textureDesc.Height = textureData.height;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(targetTexture.buffer, 0, subresources.size());

    // Temporary upload buffer
    CD3DX12_HEAP_PROPERTIES heapPropertiesUpload(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferUloadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    
    assert(m_textureUploadIndex < MAX_TEXTURE_UPLOADS);
    ID3D12Resource** tempHeap = &m_textureUploadHeaps[m_textureUploadIndex];
    m_textureUploadIndex++;

    ThrowIfFailed(m_device->CreateCommittedResource(&heapPropertiesUpload, D3D12_HEAP_FLAG_NONE, &bufferUloadDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, NewComObject(comPointers, tempHeap)));
    (*tempHeap)->SetName(L"Temp Texture Upload Heap");

    UpdateSubresources(m_uploadCommandList, targetTexture.buffer, *tempHeap, 0, 0, subresources.size(), subresources.data());

    CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(targetTexture.buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    m_uploadCommandList->ResourceBarrier(1, &transition);

    // SRV for the texture
    targetTexture.handle = GetNewDescriptorHandle();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = textureData.mipmapCount;
    m_device->CreateShaderResourceView(targetTexture.buffer, &srvDesc, targetTexture.handle.cpuHandle);
}

size_t EngineCore::CreateMaterial(const size_t maxVertices, const size_t vertexStride, std::vector<Texture*> textures, ShaderDescription shaderDesc)
{
    const size_t maxByteCount = vertexStride * maxVertices;

    assert(m_materialCount < MAX_MATERIALS);
    MaterialData& data = m_materials[m_materialCount];
    size_t dataIndex = m_materialCount;
    m_materialCount++;

    data.maxVertexCount = maxVertices;
    data.vertexStride = vertexStride;
    for (int i = 0; i < textures.size(); i++)
    {
        data.textures[i] = textures[i];
    }
    data.pipeline = CreatePipeline(shaderDesc, textures.size(), 0, 0);

    // Create buffer for upload
    CD3DX12_HEAP_PROPERTIES tempHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC tempBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(maxByteCount);
    ThrowIfFailed(m_device->CreateCommittedResource(&tempHeapProperties, D3D12_HEAP_FLAG_NONE, &tempBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, NewComObject(comPointers, &data.vertexUploadBuffer)));
    data.vertexUploadBuffer->SetName(L"Vertex Upload Buffer");

    // Create real vertex buffer on gpu memory
    CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(maxByteCount);
    ThrowIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, NewComObject(comPointers, &data.vertexBuffer)));
    data.vertexBuffer->SetName(L"Vertex Buffer");

    return dataIndex;
}

D3D12_VERTEX_BUFFER_VIEW EngineCore::CreateMesh(const size_t materialIndex, const void* vertexData, const size_t vertexCount)
{
    // Update material
    MaterialData& data = m_materials[materialIndex];
    const size_t sizeInBytes = data.vertexStride * vertexCount;
    const size_t offsetInBuffer = data.vertexCount * data.vertexStride;

    D3D12_VERTEX_BUFFER_VIEW view{};
    view.BufferLocation = data.vertexBuffer->GetGPUVirtualAddress() + offsetInBuffer;
    view.StrideInBytes = data.vertexStride;
    view.SizeInBytes = sizeInBytes;

    assert(data.vertexCount + vertexCount <= data.maxVertexCount);

    // Write to temp buffer
    UINT8* pVertexDataBegin = nullptr;
    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(data.vertexUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin + offsetInBuffer, vertexData, sizeInBytes);
    data.vertexUploadBuffer->Unmap(0, nullptr);

    data.vertexCount += vertexCount;
    return view;
}

size_t EngineCore::CreateEntity(const size_t materialIndex, D3D12_VERTEX_BUFFER_VIEW& meshView, size_t boneCount, TransformHierachy* hierachy)
{
    MaterialData& material = m_materials[materialIndex];

    assert(material.entityCount < MAX_ENTITIES_PER_MATERIAL);
    EntityData* entity = material.entities[material.entityCount] = NewObject(engineArena, EntityData);
    entity->entityIndex = material.entityCount;
    material.entityCount++;

    entity->materialIndex = materialIndex;
    entity->vertexBufferView = meshView;

    entity->boneCount = boneCount;
    entity->transformHierachy = hierachy;

    CreateConstantBuffers<EntityConstantBuffer>(entity->constantBuffer, std::format(L"Entity {} Constant Buffer", entity->entityIndex).c_str());
    CreateConstantBuffers<BoneMatricesBuffer>(entity->boneConstantBuffer, std::format(L"Entity {} Bone Buffer", entity->entityIndex).c_str());

    return entity->entityIndex;
}

void EngineCore::UploadVertices()
{
    for (int i = 0; i < m_materialCount; i++)
    {
        MaterialData& data = m_materials[i];
        m_uploadCommandList->CopyResource(data.vertexBuffer, data.vertexUploadBuffer);
        CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(data.vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        m_uploadCommandList->ResourceBarrier(1, &transition);
    }

    m_scheduleUpload = true;
}

void EngineCore::OnUpdate()
{
    std::chrono::steady_clock::time_point now = std::chrono::high_resolution_clock::now();
    double secondsSinceStart = NanosecondsToSeconds(now - m_startTime);
    m_updateDeltaTime = NanosecondsToSeconds(now - m_frameStartTime);
    m_frameStartTime = now;

    m_sceneConstantBuffer.data.time = { static_cast<float>(secondsSinceStart) };

    NewImguiFrame();
    UpdateImgui(this);

    if (!m_gameStarted)
    {
        BeginProfile("Start Game", ImColor::HSV(0.f, 0.f, 1.f));
        m_gameStarted = true;
        m_game->StartGame(*this);
        EndProfile("Start Game");
    }

    BeginProfile("Reset Upload CB", ImColor::HSV(.33f, .33f, 1.f));
    if (!m_scheduleUpload)
    {
        ThrowIfFailed(m_uploadCommandAllocators[m_frameIndex]->Reset());
        ThrowIfFailed(m_uploadCommandList->Reset(m_uploadCommandAllocators[m_frameIndex], nullptr));
    }
    EndProfile("Reset Upload CB");

    m_game->UpdateGame(*this);
    ThrowIfFailed(m_uploadCommandList->Close());
}

void EngineCore::OnRender()
{
    BeginProfile("Commands", ImColor::HSV(.0, .5, 1.));

    if (m_wantedWindowMode != m_windowMode)
    {
        ApplyWindowMode();
    }

    if (m_scheduleUpload)
    {
        m_scheduleUpload = false;
        ID3D12CommandList* uploadList = m_uploadCommandList;
        m_commandQueue->ExecuteCommandLists(1, &uploadList);
    }

#ifdef START_WITH_XR
    m_xrState.BeginFrame();
#endif
    PopulateCommandList();
#ifdef START_WITH_XR
    m_xrState.EndFrame();
#endif
    EndProfile("Commands");

    BeginProfile("Present", ImColor::HSV(.2f, .5f, 1.f));
    UINT presentFlags = (m_tearingSupport && m_windowMode != WindowMode::Fullscreen && !m_useVsync) ? DXGI_PRESENT_ALLOW_TEARING : 0;

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(m_useVsync ? 1 : 0, presentFlags));
    EndProfile("Present");

    BeginProfile("Frame Fence", ImColor::HSV(.4f, .5f, 1.f));
    MoveToNextFrame();
    EndProfile("Frame Fence");

    frameArena.Reset();
}

void EngineCore::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_renderCommandAllocators[m_frameIndex]->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_renderCommandList->Reset(m_renderCommandAllocators[m_frameIndex], nullptr));
    ID3D12GraphicsCommandList* renderList = m_renderCommandList;

    m_sceneConstantBuffer.UploadData(m_frameIndex);
    m_lightConstantBuffer.UploadData(m_frameIndex);

    for (int matIndex = 0; matIndex < m_materialCount; matIndex++)
    {
        for (int entityIndex = 0; entityIndex < m_materials[matIndex].entityCount; entityIndex++)
        {
            EntityData* entityData = m_materials[matIndex].entities[entityIndex];
            entityData->constantBuffer.UploadData(m_frameIndex);
            entityData->boneConstantBuffer.UploadData(m_frameIndex);
        }
    }

    RenderShadows(renderList);

    Transition(renderList, m_shadowmap->textureResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_COPY_SOURCE);

    CopyDebugImage(renderList, m_shadowmap->textureResource);

    Transition(renderList, m_shadowmap->textureResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    ThrowIfFailed(renderList->Close());
    ID3D12CommandList* rl = reinterpret_cast<ID3D12CommandList*>(renderList);
    m_commandQueue->ExecuteCommandLists(1, &rl);

#ifdef START_WITH_XR
    for (int i = 0; i < m_xrState.m_viewCount; i++)
#endif
    {
#ifdef START_WITH_XR
        SwapchainResult swapchainResult = m_xrState.GetSwapchain(i);
        swapchainResult.context->ResetCommandAllocator();

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), swapchainResult.swapchainImageIndex * 2 + i, m_rtvDescriptorSize);
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_depthStencilHeap->GetCPUDescriptorHandleForHeapStart(), swapchainResult.swapchainImageIndex * 2 + i, m_dsvDescriptorSize);

        D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
        renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        renderTargetViewDesc.Format = DISPLAY_FORMAT;
        m_device->CreateRenderTargetView(swapchainResult.swapchain->texture, &renderTargetViewDesc, rtvHandle);

        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
        depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthStencilViewDesc.Format = DEPTH_BUFFER_FORMAT;
        m_device->CreateDepthStencilView(swapchainResult.context->GetDepthStencilTexture(swapchainResult.swapchain->texture, comPointers), &depthStencilViewDesc, dsvHandle);
        
        ComPtr<ID3D12GraphicsCommandList> cmdList;
        ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, swapchainResult.context->GetCommandAllocator(), nullptr, __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void**>(cmdList.ReleaseAndGetAddressOf())));
        renderList = cmdList.Get();
#else
        ID3D12Resource* renderTarget = m_renderTargets[m_frameIndex];

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_depthStencilHeap->GetCPUDescriptorHandleForHeapStart());
        ThrowIfFailed(renderList->Reset(m_renderCommandAllocators[m_frameIndex], nullptr));
        
        Transition(renderList, renderTarget, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
#endif

        RenderScene(renderList, rtvHandle, dsvHandle);
        if (renderBones) RenderBones(renderList, rtvHandle, dsvHandle);
        RenderWireframe(renderList, rtvHandle, dsvHandle);

        DrawImgui(renderList, &rtvHandle);

#ifndef START_WITH_XR
        // Indicate that the back buffer will now be used to present.
        CD3DX12_RESOURCE_BARRIER barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
        renderList->ResourceBarrier(1, &barrierToPresent);
#endif

        ThrowIfFailed(renderList->Close());
        ID3D12CommandList* rl = reinterpret_cast<ID3D12CommandList*>(renderList);
        m_commandQueue->ExecuteCommandLists(1, &rl);

#ifdef START_WITH_XR
        m_xrState.ReleaseSwapchain(i, swapchainResult.context);
#endif
    }
}

void EngineCore::RenderShadows(ID3D12GraphicsCommandList* renderList)
{
    // Set pipeline
    renderList->SetPipelineState(m_shadowConfig->pipelineState);
    renderList->SetGraphicsRootSignature(m_shadowConfig->rootSignature);
    renderList->RSSetViewports(1, &m_shadowmap->viewport);
    renderList->RSSetScissorRects(1, &m_shadowmap->scissorRect);

    // Load heaps
    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap };
    renderList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    renderList->SetGraphicsRootDescriptorTable(SCENE, m_sceneConstantBuffer.handles[m_frameIndex].gpuHandle);
    renderList->SetGraphicsRootDescriptorTable(LIGHT, m_lightConstantBuffer.handles[m_frameIndex].gpuHandle);

    // Barrier to transition shadow map
    Transition(renderList, m_shadowmap->textureResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);

    CD3DX12_CPU_DESCRIPTOR_HANDLE shadowHandle(m_depthStencilHeap->GetCPUDescriptorHandleForHeapStart(), 1, m_dsvDescriptorSize);
    renderList->OMSetRenderTargets(0, nullptr, FALSE, &shadowHandle);

    // Record commands.
    renderList->ClearDepthStencilView(shadowHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    renderList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (int drawIdx = 0; drawIdx < m_materialCount; drawIdx++)
    {
        MaterialData& data = m_materials[drawIdx];
        //renderList->SetGraphicsRootDescriptorTable(DIFFUSE, data.diffuse->handle.gpuHandle);

        for (int entityIndex = 0; entityIndex < data.entityCount; entityIndex++)
        {
            EntityData* entity = data.entities[entityIndex];
            if (!entity->visible) continue;
            renderList->IASetVertexBuffers(0, 1, &entity->vertexBufferView);
            renderList->SetGraphicsRootDescriptorTable(ENTITY, entity->constantBuffer.handles[m_frameIndex].gpuHandle);
            renderList->SetGraphicsRootDescriptorTable(BONES, entity->boneConstantBuffer.handles[m_frameIndex].gpuHandle);
            renderList->DrawInstanced(entity->vertexBufferView.SizeInBytes / entity->vertexBufferView.StrideInBytes, 1, 0, 0);
        }
    }
}

void EngineCore::RenderScene(ID3D12GraphicsCommandList* renderList, CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle)
{
    renderList->RSSetViewports(1, &m_viewport);
    renderList->RSSetScissorRects(1, &m_scissorRect);

    renderList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap };
    renderList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Record commands.
    renderList->ClearRenderTargetView(rtvHandle, m_game->GetClearColor(), 0, nullptr);
    renderList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    renderList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (int drawIdx = 0; drawIdx < m_materialCount; drawIdx++)
    {
        MaterialData& data = m_materials[drawIdx];

        renderList->SetPipelineState(data.pipeline->pipelineState);
        renderList->SetGraphicsRootSignature(data.pipeline->rootSignature);

        renderList->SetGraphicsRootDescriptorTable(SCENE, m_sceneConstantBuffer.handles[m_frameIndex].gpuHandle);
        renderList->SetGraphicsRootDescriptorTable(LIGHT, m_lightConstantBuffer.handles[m_frameIndex].gpuHandle);
        renderList->SetGraphicsRootDescriptorTable(SHADOWMAP, m_shadowmap->shaderResourceViewHandle.gpuHandle);

        for (int textureIdx = 0; textureIdx < data.pipeline->textureSlotCount; textureIdx++)
        {
            renderList->SetGraphicsRootDescriptorTable(CUSTOM_START + textureIdx, data.textures[textureIdx]->handle.gpuHandle);
        }

        for (int entityIndex = 0; entityIndex < data.entityCount; entityIndex++)
        {
            EntityData* entity = data.entities[entityIndex];
            if (!entity->visible || entity->wireframe) continue;
            renderList->IASetVertexBuffers(0, 1, &entity->vertexBufferView);
            renderList->SetGraphicsRootDescriptorTable(ENTITY, entity->constantBuffer.handles[m_frameIndex].gpuHandle);
            renderList->SetGraphicsRootDescriptorTable(BONES, entity->boneConstantBuffer.handles[m_frameIndex].gpuHandle);
            renderList->DrawInstanced(entity->vertexBufferView.SizeInBytes / entity->vertexBufferView.StrideInBytes, 1, 0, 0);
        }
    }
}

void EngineCore::RenderBones(ID3D12GraphicsCommandList* renderList, CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle)
{
    // Set necessary state.
    renderList->SetPipelineState(m_boneDebugConfig->pipelineState);
    renderList->SetGraphicsRootSignature(m_boneDebugConfig->rootSignature);
    renderList->RSSetViewports(1, &m_viewport);
    renderList->RSSetScissorRects(1, &m_scissorRect);

    // Load heaps
    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap };
    renderList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    renderList->SetGraphicsRootDescriptorTable(SCENE, m_sceneConstantBuffer.handles[m_frameIndex].gpuHandle);
    renderList->SetGraphicsRootDescriptorTable(LIGHT, m_lightConstantBuffer.handles[m_frameIndex].gpuHandle);

    renderList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Record commands.
    renderList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    renderList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (int drawIdx = 0; drawIdx < m_materialCount; drawIdx++)
    {
        MaterialData& data = m_materials[drawIdx];

        for (int entityIndex = 0; entityIndex < data.entityCount; entityIndex++)
        {
            EntityData* entity = data.entities[entityIndex];

            if (entity->visible)
            {
                for (int boneIndex = 0; boneIndex < entity->boneCount; boneIndex++)
                {
                    renderList->IASetVertexBuffers(0, 1, &cubeVertexView);
                    renderList->SetGraphicsRootDescriptorTable(ENTITY, entity->constantBuffer.handles[m_frameIndex].gpuHandle);
                    renderList->SetGraphicsRootDescriptorTable(BONES, entity->boneConstantBuffer.handles[m_frameIndex].gpuHandle);
                    renderList->SetGraphicsRoot32BitConstant(CUSTOM_START, boneIndex, 0);
                    renderList->DrawInstanced(cubeVertexView.SizeInBytes / cubeVertexView.StrideInBytes, 1, 0, 0);
                }
            }
        }
    }
}

void EngineCore::RenderWireframe(ID3D12GraphicsCommandList* renderList, CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle, CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle)
{
    // Set necessary state.
    renderList->SetPipelineState(m_wireframeConfig->pipelineState);
    renderList->SetGraphicsRootSignature(m_wireframeConfig->rootSignature);
    renderList->RSSetViewports(1, &m_viewport);
    renderList->RSSetScissorRects(1, &m_scissorRect);

    // Load heaps
    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap };
    renderList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    renderList->SetGraphicsRootDescriptorTable(SCENE, m_sceneConstantBuffer.handles[m_frameIndex].gpuHandle);
    renderList->SetGraphicsRootDescriptorTable(LIGHT, m_lightConstantBuffer.handles[m_frameIndex].gpuHandle);
    renderList->SetGraphicsRootDescriptorTable(SHADOWMAP, m_shadowmap->shaderResourceViewHandle.gpuHandle);

    renderList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Record commands.
    renderList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    renderList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (int drawIdx = 0; drawIdx < m_materialCount; drawIdx++)
    {
        MaterialData& data = m_materials[drawIdx];

        for (int entityIndex = 0; entityIndex < data.entityCount; entityIndex++)
        {
            EntityData* entity = data.entities[entityIndex];

            if (entity->visible && renderAABB)
            {
                renderList->IASetVertexBuffers(0, 1, &cubeVertexView);
                renderList->SetGraphicsRootDescriptorTable(ENTITY, entity->constantBuffer.handles[m_frameIndex].gpuHandle);
                renderList->SetGraphicsRootDescriptorTable(BONES, entity->boneConstantBuffer.handles[m_frameIndex].gpuHandle);
                renderList->DrawInstanced(cubeVertexView.SizeInBytes / cubeVertexView.StrideInBytes, 1, 0, 0);
            }

            if (entity->visible && entity->wireframe)
            {
                renderList->IASetVertexBuffers(0, 1, &entity->vertexBufferView);
                renderList->SetGraphicsRootDescriptorTable(ENTITY, entity->constantBuffer.handles[m_frameIndex].gpuHandle);
                renderList->SetGraphicsRootDescriptorTable(BONES, entity->boneConstantBuffer.handles[m_frameIndex].gpuHandle);
                renderList->DrawInstanced(entity->vertexBufferView.SizeInBytes / entity->vertexBufferView.StrideInBytes, 1, 0, 0);
            }
        }
    }
}

// Wait for pending GPU work to complete.
void EngineCore::WaitForGpu()
{
    // Schedule a Signal command in the queue.
    ThrowIfFailed(m_commandQueue->Signal(m_fence, m_fenceValues[m_frameIndex]));

    // Wait until the fence has been processed.
    ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

    // Increment the fence value for the current frame.
    m_fenceValues[m_frameIndex]++;
}

// Prepare to render the next frame.
void EngineCore::MoveToNextFrame()
{
    // Schedule a Signal command in the queue.
    const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
    ThrowIfFailed(m_commandQueue->Signal(m_fence, currentFenceValue));

    // Update the frame index.
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // If the next frame is not ready to be rendered yet, wait until it is ready.
    const UINT64 completedValue = m_fence->GetCompletedValue();
    if (completedValue < m_fenceValues[m_frameIndex])
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }

    // Set the fence value for the next frame.
    m_fenceValues[m_frameIndex] = currentFenceValue + 1;
}

void EngineCore::OnResize(UINT width, UINT height)
{
    // TODO:
    return;

    // Flush all current GPU commands.
    WaitForGpu();

    m_width = width;
    m_height = height;

    if (m_width == 0 || m_height == 0) return;

    m_aspectRatio = (float)m_width / (float)m_height;
    m_viewport = CD3DX12_VIEWPORT{ 0.f, 0.f, static_cast<float>(m_width), static_cast<float>(m_height) };
    m_scissorRect = CD3DX12_RECT{ 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };

    // Release the resources holding references to the swap chain (requirement of
    // IDXGISwapChain::ResizeBuffers) and reset the frame fence values to the
    // current fence value.
    comPointersSizeDependent.Clear();

    for (UINT n = 0; n < FrameCount; n++)
    {
        m_fenceValues[n] = m_fenceValues[m_frameIndex];
    }

    // Resize the swap chain to the desired dimensions.
    DXGI_SWAP_CHAIN_DESC desc = {};
    m_swapChain->GetDesc(&desc);
    ThrowIfFailed(m_swapChain->ResizeBuffers(FrameCount, m_width, m_height, desc.BufferDesc.Format, desc.Flags));

    BOOL fullscreenState;
    ThrowIfFailed(m_swapChain->GetFullscreenState(&fullscreenState, nullptr));
    if (fullscreenState)
    {
        m_windowMode = WindowMode::Fullscreen;
        m_wantedWindowMode = m_windowMode;
    }
    else if (!fullscreenState && m_windowMode == WindowMode::Fullscreen)
    {
        m_windowMode = WindowMode::Windowed;
        m_wantedWindowMode = m_windowMode;
    }

    // Reset the frame index to the current back buffer index.
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    LoadSizeDependentResources();
}

void EngineCore::OnShaderReload()
{
    WaitForGpu();

    std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;

    for (int i = 0; i < m_materialCount; i++)
    {
        MaterialData& material = m_materials[i];
        CreatePipelineState(material.pipeline);

        if (FAILED(material.pipeline->creationError))
        {
            _com_error err(material.pipeline->creationError);

            m_shaderError.append("Failed to create material pipeline state: ");
            m_shaderError.append(utf8_conv.to_bytes(err.ErrorMessage()));
            return;
        }
    }

    CreatePipelineState(m_shadowConfig);
    if (FAILED(m_shadowConfig->creationError))
    {
        _com_error err(m_shadowConfig->creationError);

        m_shaderError.append("Failed to create shadow pipeline state: ");
        m_shaderError.append(utf8_conv.to_bytes(err.ErrorMessage()));
        return;
    }

    CreatePipelineState(m_boneDebugConfig);
    if (FAILED(m_boneDebugConfig->creationError))
    {
        _com_error err(m_boneDebugConfig->creationError);

        m_shaderError.append("Failed to create bone pipeline state: ");
        m_shaderError.append(utf8_conv.to_bytes(err.ErrorMessage()));
        return;
    }

    CreatePipelineState(m_wireframeConfig);
    if (FAILED(m_wireframeConfig->creationError))
    {
        _com_error err(m_wireframeConfig->creationError);

        m_shaderError.append("Failed to create wireframe pipeline state: ");
        m_shaderError.append(utf8_conv.to_bytes(err.ErrorMessage()));
        return;
    }
}

void EngineCore::OnDestroy()
{
    WaitForGpu();
    m_wantedWindowMode = WindowMode::Windowed;
    ApplyWindowMode();
    CloseHandle(m_fenceEvent);

    DestroyImgui();
#ifdef START_WITH_XR
    m_xrState.ShutdownXR();
#endif

    comPointersTextureUpload.Clear();
    comPointersSizeDependent.Clear();
    comPointers.Clear();
}

// Determines whether tearing support is available for fullscreen borderless windows.
void EngineCore::CheckTearingSupport()
{
#ifndef PIXSUPPORT
    ComPtr<IDXGIFactory6> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    BOOL allowTearing = FALSE;
    if (SUCCEEDED(hr))
    {
        hr = factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
    }

    m_tearingSupport = SUCCEEDED(hr) && allowTearing;
#else
    m_tearingSupport = TRUE;
#endif
}

void EngineCore::ToggleWindowMode()
{
    if (m_windowMode == WindowMode::Windowed)
    {
        m_wantedWindowMode = m_wantBorderless ? WindowMode::Borderless : WindowMode::Fullscreen;
    }
    else
    {
        m_wantedWindowMode = WindowMode::Windowed;
    }
}

void EngineCore::ApplyWindowMode()
{
    if (m_windowMode == m_wantedWindowMode) return;
    m_windowMode = m_wantedWindowMode;

    BOOL isInFullscreen = false;
    ThrowIfFailed(m_swapChain->GetFullscreenState(&isInFullscreen, nullptr));

    if (m_windowMode == WindowMode::Windowed)
    {
        ThrowIfFailed(m_swapChain->SetFullscreenState(FALSE, nullptr));

        // Restore the window's attributes and size.
        SetWindowLong(m_hwnd, GWL_STYLE, m_windowStyle);

        LONG width = m_windowRect.right - m_windowRect.left;
        LONG height = m_windowRect.bottom - m_windowRect.top;

        SetWindowPos(
            m_hwnd,
            HWND_NOTOPMOST,
            m_windowRect.left,
            m_windowRect.top,
            width,
            height,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(m_hwnd, SW_NORMAL);

        OnResize(width, height);
    }
    else if (m_windowMode == WindowMode::Borderless)
    {
        ThrowIfFailed(m_swapChain->SetFullscreenState(FALSE, nullptr));

        // Save the old window rect so we can restore it when exiting fullscreen mode.
        GetWindowRect(m_hwnd, &m_windowRect);

        // Make the window borderless so that the client area can fill the screen.
        SetWindowLong(m_hwnd, GWL_STYLE, m_windowStyle & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

        RECT fullscreenWindowRect;
        try
        {
            if (m_swapChain)
            {
                // Get the settings of the display on which the app's window is currently displayed
                ComPtr<IDXGIOutput> pOutput;
                ThrowIfFailed(m_swapChain->GetContainingOutput(&pOutput));
                DXGI_OUTPUT_DESC Desc;
                ThrowIfFailed(pOutput->GetDesc(&Desc));
                fullscreenWindowRect = Desc.DesktopCoordinates;
            }
            else
            {
                // Fallback to EnumDisplaySettings implementation
                throw HrException(S_FALSE);
            }
        }
        catch (HrException& e)
        {
            UNREFERENCED_PARAMETER(e);

            // Get the settings of the primary display
            DEVMODE devMode = {};
            devMode.dmSize = sizeof(DEVMODE);
            EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode);

            fullscreenWindowRect = {
                devMode.dmPosition.x,
                devMode.dmPosition.y,
                devMode.dmPosition.x + static_cast<LONG>(devMode.dmPelsWidth),
                devMode.dmPosition.y + static_cast<LONG>(devMode.dmPelsHeight)
            };
        }

        SetWindowPos(
            m_hwnd,
            HWND_TOPMOST,
            fullscreenWindowRect.left,
            fullscreenWindowRect.top,
            fullscreenWindowRect.right,
            fullscreenWindowRect.bottom,
            SWP_FRAMECHANGED | SWP_NOACTIVATE);

        ShowWindow(m_hwnd, SW_MAXIMIZE);
        OnResize(fullscreenWindowRect.right - fullscreenWindowRect.left, fullscreenWindowRect.bottom - fullscreenWindowRect.top);
    }
    else if (m_windowMode == WindowMode::Fullscreen)
    {
        // Save the old window rect so we can restore it when exiting fullscreen mode.
        GetWindowRect(m_hwnd, &m_windowRect);

        // Get the settings of the display on which the app's window is currently displayed
        ComPtr<IDXGIOutput> pOutput;
        ThrowIfFailed(m_swapChain->GetContainingOutput(&pOutput));
        DXGI_MODE_DESC modes[1024];
        UINT numModes;
        ThrowIfFailed(pOutput->GetDisplayModeList(DISPLAY_FORMAT, 0, &numModes, modes));

        int bestModeIndex = -1;
        UINT bestWidth = 0;
        double bestRefreshRate = 0.;
        for (int i = 0; i < static_cast<int>(numModes); i++)
        {
            double refreshRate = static_cast<double>(modes[i].RefreshRate.Numerator) / static_cast<double>(modes[i].RefreshRate.Denominator);
            if (refreshRate >= bestRefreshRate)
            {
                bestRefreshRate = refreshRate;
                if (modes[i].Width > bestWidth)
                {
                    bestWidth = modes[i].Width;
                    bestModeIndex = i;
                }
            }
        }

        if (bestModeIndex == -1) throw std::exception();

        m_swapChain->ResizeTarget(&modes[bestModeIndex]);
        if (FAILED(m_swapChain->SetFullscreenState(true, nullptr)))
        {
            OutputDebugString(L"Fullscreen transition failed");
            assert(false);
        }
        else
        {
            OnResize(modes[bestModeIndex].Width, modes[bestModeIndex].Height);
        }
    }
}

void EngineCore::BeginProfile(std::string name, ImColor color)
{
    m_profilerTaskData.emplace_back(TimeSinceFrameStart(), 0, name, color);
    m_profilerTasks.emplace(name, m_profilerTaskData.size() - 1);
}

void EngineCore::EndProfile(std::string name)
{
    assert(m_profilerTasks.contains(name));
    m_profilerTaskData[m_profilerTasks[name]].endTime = TimeSinceFrameStart();
}

double EngineCore::TimeSinceStart()
{
    return NanosecondsToSeconds(std::chrono::high_resolution_clock::now() - m_startTime);
}

double EngineCore::TimeSinceFrameStart()
{
    return NanosecondsToSeconds(std::chrono::high_resolution_clock::now() - m_frameStartTime);
}

double NanosecondsToSeconds(std::chrono::nanoseconds clock)
{
    return clock.count() / 1e+9;
}
