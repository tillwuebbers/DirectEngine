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

#include "UI.h"
#include "EngineCore.h"
#include "../directx-tex/DDSTextureLoader12.h"

EngineCore::EngineCore(UINT width, UINT height, CreateGameFunc gameFunc) :
    m_frameIndex(0),
    m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
    m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height)),
    m_fenceValues{},
    m_rtvDescriptorSize(0),
    m_width(width),
    m_height(height),
    m_aspectRatio((float)width / (float)height)
{
    m_game = gameFunc(engineArena, configArena, levelArena);
}

void EngineCore::OnInit(HINSTANCE hInst, int nCmdShow, WNDPROC wndProc)
{
    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));

    // window
    CheckTearingSupport();
    const wchar_t* windowClassName = L"DX12WindowClass";
    RegisterWindowClass(hInst, windowClassName, wndProc);
    CreateGameWindow(windowClassName, hInst, m_width, m_height);

    // directx
    LoadPipeline(nullptr);
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

// Load the rendering pipeline dependencies.
void EngineCore::LoadPipeline(LUID* requiredLuid)
{
    UINT dxgiFactoryFlags = 0;

#if ISDEBUG
    {
        ComPtr<ID3D12Debug> debugController;
        ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            
            ThrowIfFailed(debugController->QueryInterface(IID_PPV_ARGS(&debugController1)));
            //debugController1->SetEnableGPUBasedValidation(true);

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

#if ISDEBUG
    ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
    m_device->QueryInterface(IID_PPV_ARGS(&infoQueue));

    if (infoQueue != nullptr)
    {
        D3D12_MESSAGE_ID hide[] =
        {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_REFLECTSHAREDPROPERTIES_INVALIDOBJECT,
        };
        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(hide);
        filter.DenyList.pIDList = hide;
        infoQueue->AddStorageFilterEntries(&filter);
    }
#endif

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
    m_frameWaitableObject = m_swapChain->GetFrameLatencyWaitableObject();
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount * 2 + 2;
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
        dsvHeapDesc.NumDescriptors = 5; // Main, shadow, msaa, ...
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
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DISPLAY_FORMAT;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, NewComObject(comPointersSizeDependent, &m_renderTargets[n])));
            m_renderTargets[n]->SetName(std::format(L"Engine Render Target {}", n).c_str());

            m_swapchainRtvHandles[n] = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), n, m_rtvDescriptorSize);
            m_device->CreateRenderTargetView(m_renderTargets[n], &rtvDesc, m_swapchainRtvHandles[n]);


            if (m_msaaEnabled)
            {
                D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS levels = { DISPLAY_FORMAT, m_msaaSampleCount };
                ThrowIfFailed(m_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &levels, sizeof(levels)));
                assert(levels.NumQualityLevels > 0);

                CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
                CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
                    DISPLAY_FORMAT,
                    m_width,
                    m_height,
                    1,
                    1,
                    m_msaaSampleCount,
                    0,
                    D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);

                m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, NewComObject(comPointersSizeDependent, &m_msaaRenderTargets[n]));
                m_msaaRenderTargets[n]->SetName(std::format(L"MSAA Render Target {}", n).c_str());

                D3D12_RENDER_TARGET_VIEW_DESC msaaRtvDesc = {};
                msaaRtvDesc.Format = DISPLAY_FORMAT;
                msaaRtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;

                m_msaaRtvHandles[n] = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), n + FrameCount, m_rtvDescriptorSize);
                m_device->CreateRenderTargetView(m_msaaRenderTargets[n], &msaaRtvDesc, m_msaaRtvHandles[n]);
            }
        }
    }

    // depth/stencil view
    m_swapchainDsvHandle = CreateDepthStencilView(m_width, m_height, comPointersSizeDependent, &m_depthStencilBuffer, 0);
    if (m_msaaEnabled)
    {
		m_msaaDsvHandle = CreateDepthStencilView(m_width, m_height, comPointersSizeDependent, &m_msaaDepthStencilBuffer, 1, m_msaaSampleCount);
	}
}

CD3DX12_CPU_DESCRIPTOR_HANDLE EngineCore::CreateDepthStencilView(UINT width, UINT height, ComStack& comStack, ID3D12Resource** bufferTarget, int fixedOffset, UINT sampleCount)
{
    assert(bufferTarget != nullptr);

    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
    depthStencilDesc.Format = DEPTH_BUFFER_FORMAT;
    depthStencilDesc.ViewDimension = sampleCount > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS : D3D12_DSV_DIMENSION_TEXTURE2D;
    depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DEPTH_BUFFER_FORMAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES heapType(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC depthTexture = CD3DX12_RESOURCE_DESC::Tex2D(DEPTH_BUFFER_FORMAT_TYPELESS, width, height, 1, sampleCount > 1 ? 1 : 0, sampleCount, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    ThrowIfFailed(m_device->CreateCommittedResource(&heapType, D3D12_HEAP_FLAG_NONE, &depthTexture, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue, NewComObject(comStack, bufferTarget)));
    (*bufferTarget)->SetName(L"Depth/Stencil Buffer");

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle;
    if (fixedOffset >= 0)
    {
        dsvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_depthStencilHeap->GetCPUDescriptorHandleForHeapStart(), fixedOffset, m_dsvDescriptorSize);
    }
    else
    {
        dsvHandle = GetNewDSVHandle();
    }

    m_device->CreateDepthStencilView(*bufferTarget, &depthStencilDesc, dsvHandle);
    return dsvHandle;
}

void EngineCore::CreatePipeline(PipelineConfig* config, size_t constantBufferCount, size_t rootConstantCount)
{
    config->creationError = S_OK;

    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        std::vector<CD3DX12_DESCRIPTOR_RANGE1> ranges{CUSTOM_START + config->textureSlotCount + constantBufferCount};
        std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters{CUSTOM_START + config->textureSlotCount + constantBufferCount + rootConstantCount};

        ranges[SCENE]    .Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, SCENE,     0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[LIGHT]    .Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, LIGHT,     0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[ENTITY]   .Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, ENTITY,    0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[BONES]    .Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, BONES,     0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[CAMERA]   .Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, CAMERA,    0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        ranges[SHADOWMAP].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, SHADOWMAP, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
        
        rootParameters[SCENE]    .InitAsDescriptorTable(1, &ranges[SCENE],     D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[LIGHT]    .InitAsDescriptorTable(1, &ranges[LIGHT],     D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[ENTITY]   .InitAsDescriptorTable(1, &ranges[ENTITY],    D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters[BONES]    .InitAsDescriptorTable(1, &ranges[BONES],     D3D12_SHADER_VISIBILITY_VERTEX);
        rootParameters[CAMERA]   .InitAsDescriptorTable(1, &ranges[CAMERA],    D3D12_SHADER_VISIBILITY_ALL);
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
        config->rootSignature->SetName(config->shaderName.c_str());
    }

    CreatePipelineState(config);
    assert(config->creationError == S_OK);
}

void EngineCore::CreatePipelineState(PipelineConfig* config, bool hotloadShaders)
{
    INIT_TIMER(timer);

    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;

#if ISDEBUG
    if (hotloadShaders)
    {
        wchar_t exePath[MAX_PATH];
        GetModuleFileName(nullptr, exePath, MAX_PATH);
        std::wstring shaderDir = std::wstring(exePath);
        shaderDir = shaderDir.substr(0, shaderDir.find_last_of(L"\\"));
        shaderDir.append(L"/../../../../DirectEngine/shaders/");
        SetCurrentDirectory(shaderDir.c_str());

        // Enable better shader debugging with the graphics debugging tools.
        UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        std::wstring path = std::wstring(L"compile/");
        path.append(config->shaderName);
        path.append(L".hlsl");
        const wchar_t* shaderPath = path.c_str();
        Sleep(100);

	    time_t startTime = time(nullptr);
        bool canOpen = false;
        while (!canOpen)
        {
            std::ifstream fileStream(shaderPath, std::ios::in);

            canOpen = fileStream.good();
            fileStream.close();

            if (!canOpen)
            {
			    time_t elapsedSeconds = time(nullptr) - startTime;
			    if (elapsedSeconds > 5)
			    {
                    throw std::format(L"Could not open shader file {}", shaderPath).c_str();
			    }
                Sleep(100);
            }
        }

        ComPtr<ID3DBlob> vsErrors;
        ComPtr<ID3D10Blob> psErrors;

        m_shaderError.clear();
        config->creationError = D3DCompileFromFile(shaderPath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, SHADER_ENTRY_VS, SHADER_VERSION_VS, compileFlags, 0, &vertexShader, &vsErrors);

        if (vsErrors)
        {
            m_shaderError = std::format("Vertex Shader Errors:\n{}\n", (LPCSTR)vsErrors->GetBufferPointer());
            OutputDebugString(std::format(L"{}\n", shaderPath).c_str());
            OutputDebugStringA(m_shaderError.c_str());
        }
        if (FAILED(config->creationError)) return;

        config->creationError = D3DCompileFromFile(shaderPath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, SHADER_ENTRY_PS, SHADER_VERSION_PS, compileFlags, 0, &pixelShader, &psErrors);
        if (psErrors)
        {
            m_shaderError = std::format("Pixel Shader Errors:\n{}\n", (LPCSTR)psErrors->GetBufferPointer());
            OutputDebugString(std::format(L"{}\n", shaderPath).c_str());
            OutputDebugStringA(m_shaderError.c_str());
        }
        if (FAILED(config->creationError)) return;
    }
    else
#endif
    {
        std::wstring path = std::wstring(L"shaders_bin/");
        path.append(config->shaderName);

        std::wstring vsPath = path;
        vsPath.append(L".vert.cso");

        std::wstring psPath = path;
        psPath.append(L".frag.cso");

        config->creationError = D3DReadFileToBlob(vsPath.c_str(), &vertexShader);
        if (FAILED(config->creationError)) return;

        config->creationError = D3DReadFileToBlob(psPath.c_str(), &pixelShader);
        if (FAILED(config->creationError)) return;
    }

    OUTPUT_TIMERW(timer, L"Load Shaders");
    RESET_TIMER(timer);

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
    if (config->shadow)
    {
        rasterizerDesc.CullMode = D3D12_CULL_MODE_FRONT;
        rasterizerDesc.SlopeScaledDepthBias = 1.0;
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
    psoDesc.DepthStencilState.DepthWriteMask = config->ignoreDepth ? D3D12_DEPTH_WRITE_MASK_ZERO : D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = config->topologyType;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DISPLAY_FORMAT;
    psoDesc.SampleDesc.Count = config->sampleCount;
    psoDesc.DSVFormat = DEPTH_BUFFER_FORMAT;

    config->creationError = m_device->CreateGraphicsPipelineState(&psoDesc, NewComObjectReplace(comPointers, &config->pipelineState));
    config->pipelineState->SetName(config->shaderName.c_str());

    OUTPUT_TIMERW(timer, L"Create Pipeline State");
    RESET_TIMER(timer);
}

void EngineCore::LoadAssets()
{
    m_shadowConfig = NewObject(engineArena, PipelineConfig, L"shadow", 0);
    m_shadowConfig->shadow = true;
    CreatePipeline(m_shadowConfig, 0, 0);

    m_wireframeConfig = NewObject(engineArena, PipelineConfig, L"aabb", 0);
    m_wireframeConfig->wireframe = true;
    m_wireframeConfig->sampleCount = m_msaaEnabled ? m_msaaSampleCount : 1;
    CreatePipeline(m_wireframeConfig, 0, 0);
    
    m_debugLineConfig = NewObject(engineArena, PipelineConfig, L"debugline", 0);
    m_debugLineConfig->wireframe = true;
    m_debugLineConfig->ignoreDepth = true;
    m_debugLineConfig->topologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    m_debugLineConfig->sampleCount = m_msaaEnabled ? m_msaaSampleCount : 1;
    CreatePipeline(m_debugLineConfig, 0, 0);

    CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(MAX_DEBUG_LINE_VERTICES * sizeof(Vertex));
    ThrowIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, NewComObject(comPointers, &m_debugLineData.vertexBuffer)));
    m_debugLineData.vertexBuffer->SetName(L"Debug Line Vertex Buffer");

	m_debugLineData.vertexBufferView = {};
	m_debugLineData.vertexBufferView.BufferLocation = m_debugLineData.vertexBuffer->GetGPUVirtualAddress();
	m_debugLineData.vertexBufferView.StrideInBytes = sizeof(Vertex);
	m_debugLineData.vertexBufferView.SizeInBytes = MAX_DEBUG_LINE_VERTICES * sizeof(Vertex);

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

    // Shadowmap
    m_shadowmap = NewObject(engineArena, ShadowMap, DEPTH_BUFFER_FORMAT_TYPELESS, SHADOWMAP_SIZE, SHADOWMAP_SIZE);
    m_shadowmap->depthStencilViewCPU = GetNewDSVHandle();
    m_shadowmap->shaderResourceViewHandle = GetNewDescriptorHandle();
    m_shadowmap->Build(m_device, comPointers);

    // Cameras
    mainCamera = CreateCamera();

    // Render textures
    for (int i = 0; i < 2; i++)
    {
        RenderTexture* renderTexture = (m_renderTextures.newElement() = CreateRenderTexture(m_width, m_height));
        renderTexture->camera->skipRenderTextures = true;
        renderTexture->camera->name = ("Render Texture " + std::to_string(i)).c_str();
    }

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

void EngineCore::ResetLevelData()
{
    WaitForGpu();
    for (MaterialData& material : m_materials)
    {
        material.entities.clear();
    }
    comPointersLevel.Clear();
    levelArena.Reset();
}

CameraData* EngineCore::CreateCamera()
{
    CameraData& cam = m_cameras.newElement();
    CreateConstantBuffers(cam.constantBuffer, L"Camera Constant Buffer");
    return &cam;
}

RenderTexture* EngineCore::CreateRenderTexture(UINT width, UINT height)
{
    // Texture buffer
    RenderTexture* renderTexture = NewObject(engineArena, RenderTexture);
    renderTexture->width = width;
    renderTexture->height = height;
    renderTexture->format = DISPLAY_FORMAT;
    renderTexture->viewport = CD3DX12_VIEWPORT{ 0.f, 0.f, static_cast<float>(width), static_cast<float>(height) };
    renderTexture->scissorRect = CD3DX12_RECT{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
    renderTexture->camera = CreateCamera();
    renderTexture->camera->aspectRatio = static_cast<float>(width) / static_cast<float>(height);

    D3D12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(DISPLAY_FORMAT, width, height);
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    CD3DX12_CLEAR_VALUE clearColor(DISPLAY_FORMAT, m_renderTargetClearColor);

    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearColor, NewComObject(comPointers, &renderTexture->texture.buffer));
    renderTexture->texture.buffer->SetName(L"Render Texture");

    // SRV
    renderTexture->texture.handle = GetNewDescriptorHandle();
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DISPLAY_FORMAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    m_device->CreateShaderResourceView(renderTexture->texture.buffer, &srvDesc, renderTexture->texture.handle.cpuHandle);

    // DSV
    renderTexture->dsvHandle = CreateDepthStencilView(width, height, comPointers, &renderTexture->dsvBuffer, -1, m_msaaEnabled ? m_msaaSampleCount : 1);
    renderTexture->rtvHandle = GetNewRTVHandle();

    if (m_msaaEnabled)
    {
        // MSAA Buffer
        D3D12_RESOURCE_DESC textureDescMsaa = CD3DX12_RESOURCE_DESC::Tex2D(DISPLAY_FORMAT, width, height);
        textureDescMsaa.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        textureDescMsaa.SampleDesc.Count = m_msaaSampleCount;
        textureDescMsaa.MipLevels = 1;

        CD3DX12_HEAP_PROPERTIES heapPropertiesMsaa(D3D12_HEAP_TYPE_DEFAULT);
        m_device->CreateCommittedResource(&heapPropertiesMsaa, D3D12_HEAP_FLAG_NONE, &textureDescMsaa, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, &clearColor, NewComObject(comPointers, &renderTexture->msaaBuffer));
        renderTexture->msaaBuffer->SetName(L"Render Texture MSAA");

        // RTV
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DISPLAY_FORMAT;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
        m_device->CreateRenderTargetView(renderTexture->msaaBuffer, &rtvDesc, renderTexture->rtvHandle);
    }
    else
    {
        // RTV
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DISPLAY_FORMAT;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        m_device->CreateRenderTargetView(renderTexture->texture.buffer, &rtvDesc, renderTexture->rtvHandle);
    }

    return renderTexture;
}

Texture* EngineCore::CreateTexture(const std::wstring& filePath)
{
    std::string textureId(filePath.begin(), filePath.end());
    for (Texture& tex : m_textures)
	{
        if (tex.name == textureId)
        {
			return &tex;
		}
	}

    Texture& texture = m_textures.newElement();
    texture.name = textureId;

    TextureData header = ParseDDSHeader(filePath.c_str());
    std::unique_ptr<uint8_t[]> data{};
    std::vector<D3D12_SUBRESOURCE_DATA> subresources{};
    CHECK_HRCMD(LoadDDSTextureFromFile(m_device, filePath.c_str(), &texture.buffer, data, subresources));
    comPointers.AddPointer((void**)&texture.buffer);
    UploadTexture(header, subresources, texture);

    return &texture;
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

size_t EngineCore::CreateMaterial(const size_t maxVertices, const size_t vertexStride, const std::vector<Texture*>& textures, const std::wstring& shaderName)
{
    INIT_TIMER(timer);

    const size_t maxByteCount = vertexStride * maxVertices;

    size_t dataIndex = m_materials.size;
    MaterialData& data = m_materials.newElement();

    data.maxVertexCount = maxVertices;
    data.vertexStride = vertexStride;
    for (Texture* tex : textures)
    {
        data.textures.newElement() = tex;
    }
    data.name = std::string(shaderName.begin(), shaderName.end());

    OUTPUT_TIMERW(timer, L"Init");
    RESET_TIMER(timer);

    data.pipeline = NewObject(engineArena, PipelineConfig, shaderName, textures.size());
    if (m_msaaEnabled) data.pipeline->sampleCount = m_msaaSampleCount;
    CreatePipeline(data.pipeline, 0, 0);

    OUTPUT_TIMERW(timer, L"Pipeline");
    RESET_TIMER(timer);

    // Create buffer for upload
    CD3DX12_HEAP_PROPERTIES tempHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC tempBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(maxByteCount);
    ThrowIfFailed(m_device->CreateCommittedResource(&tempHeapProperties, D3D12_HEAP_FLAG_NONE, &tempBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, NewComObject(comPointers, &data.vertexUploadBuffer)));
    data.vertexUploadBuffer->SetName(L"Vertex Upload Buffer");

    // Create real vertex buffer on gpu memory
    CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(maxByteCount);
    ThrowIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, NewComObject(comPointers, &data.vertexBuffer)));
    data.vertexBuffer->SetName(L"Vertex Buffer");

    OUTPUT_TIMERW(timer, L"Buffers");
    RESET_TIMER(timer);

    return dataIndex;
}

D3D12_VERTEX_BUFFER_VIEW EngineCore::CreateMesh(const size_t materialIndex, const void* vertexData, const size_t vertexCount, const uint64_t id)
{
    if (auto mesh = m_meshes.find(id); mesh != m_meshes.end())
	{
		return mesh->second;
	}

    // Update material
    MaterialData& data = m_materials.at(materialIndex);
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
    m_meshes.try_emplace(id, view);

    return view;
}

size_t EngineCore::CreateEntity(const size_t materialIndex, D3D12_VERTEX_BUFFER_VIEW& meshView)
{
    MaterialData& material = m_materials.at(materialIndex);

    EntityData* entity = material.entities.newElement() = NewObject(levelArena, EntityData);
    entity->entityIndex = material.entities.size - 1;
    entity->materialIndex = materialIndex;
    entity->vertexBufferView = meshView;

    CreateConstantBuffers<EntityConstantBuffer>(entity->constantBuffer, std::format(L"Entity {} Constant Buffer", entity->entityIndex).c_str(), &comPointersLevel);
    CreateConstantBuffers<BoneMatricesBuffer>(entity->boneConstantBuffer, std::format(L"Entity {} Bone Buffer", entity->entityIndex).c_str(), &comPointersLevel);
    CreateConstantBuffers<BoneMatricesBuffer>(entity->firstPersonBoneConstantBuffer, std::format(L"Entity {} First Person Bone Buffer", entity->entityIndex).c_str(), &comPointersLevel);

    return entity->entityIndex;
}

void EngineCore::UploadVertices()
{
    for (MaterialData& data : m_materials)
    {
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

    if (m_resetLevel)
    {
        m_resetLevel = false;
        ResetLevelData();
    }
}

void EngineCore::OnRender()
{
    BeginProfile("Prepare Commands", ImColor::HSV(.0, .2, 1.));
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

    EndProfile("Prepare Commands");

    PopulateCommandList();

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

void EngineCore::ExecCommandList(ID3D12GraphicsCommandList* commandList)
{
    ThrowIfFailed(commandList->Close());
    ID3D12CommandList* list = reinterpret_cast<ID3D12CommandList*>(commandList);
    m_commandQueue->ExecuteCommandLists(1, &list);
}

void EngineCore::PopulateCommandList()
{
    // Desktop Render
    BeginProfile("Render Setup", ImColor::HSV(.4, .05, 1.));

    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_renderCommandAllocators[m_frameIndex]->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_renderCommandList->Reset(m_renderCommandAllocators[m_frameIndex], nullptr));

    // Upload Data
    m_sceneConstantBuffer.UploadData(m_frameIndex);
    m_lightConstantBuffer.UploadData(m_frameIndex);

    for (MaterialData& data : m_materials)
    {
        for (EntityData* entityData : data.entities)
        {
            entityData->constantBuffer.UploadData(m_frameIndex);
            entityData->boneConstantBuffer.UploadData(m_frameIndex);
        }
    }

    mainCamera->aspectRatio = m_aspectRatio;
    for (CameraData& cameraData : m_cameras)
	{
		cameraData.constantBuffer.UploadData(m_frameIndex);
	}

    // Setup Handles
    D3D12_CPU_DESCRIPTOR_HANDLE mainRtvHandle = m_msaaEnabled ? m_msaaRtvHandles[m_frameIndex] : m_swapchainRtvHandles[m_frameIndex];
    D3D12_CPU_DESCRIPTOR_HANDLE mainDsvHandle = m_msaaEnabled ? m_msaaDsvHandle : m_swapchainDsvHandle;
    ID3D12Resource* renderTargetMsaa = m_msaaRenderTargets[m_frameIndex];
    ID3D12Resource* renderTargetWindow = m_renderTargets[m_frameIndex];
    EndProfile("Render Setup");

    // Shadows
    BeginProfile("Render Shadows", ImColor::HSV(.5, .1, 1.));
    RenderShadows(m_renderCommandList);
    ExecCommandList(m_renderCommandList);
    EndProfile("Render Shadows");

    // Render Textures
    if (m_renderTextureEnabled)
    {
        BeginProfile("RenderTextures", ImColor::HSV(.6, .2, 1.));

        for (RenderTexture* renderTexture : m_renderTextures)
        {
            ThrowIfFailed(m_renderCommandList->Reset(m_renderCommandAllocators[m_frameIndex], nullptr));

            m_renderCommandList->RSSetViewports(1, &renderTexture->viewport);
            m_renderCommandList->RSSetScissorRects(1, &renderTexture->scissorRect);

            if (m_msaaEnabled)
            {
                Transition(m_renderCommandList, renderTexture->msaaBuffer, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            }
            else
            {
                Transition(m_renderCommandList, renderTexture->texture.buffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
            }

            m_renderCommandList->ClearRenderTargetView(renderTexture->rtvHandle, m_game->GetClearColor(), 0, nullptr);
            m_renderCommandList->ClearDepthStencilView(renderTexture->dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
            RenderScene(m_renderCommandList, renderTexture->rtvHandle, renderTexture->dsvHandle, renderTexture->camera);

            if (m_msaaEnabled)
            {
                Transition(m_renderCommandList, renderTexture->msaaBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
                Transition(m_renderCommandList, renderTexture->texture.buffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RESOLVE_DEST);
                m_renderCommandList->ResolveSubresource(renderTexture->texture.buffer, 0, renderTexture->msaaBuffer, 0, DISPLAY_FORMAT);
                Transition(m_renderCommandList, renderTexture->texture.buffer, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }
            else
            {
                Transition(m_renderCommandList, renderTexture->texture.buffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }

            ExecCommandList(m_renderCommandList);
        }

        EndProfile("RenderTextures");
    }

    BeginProfile("Render Main", ImColor::HSV(.7, .3, 1.));

    ThrowIfFailed(m_renderCommandList->Reset(m_renderCommandAllocators[m_frameIndex], nullptr));
    Transition(m_renderCommandList, m_renderTextures[0]->texture.buffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);
    //CopyDebugImage(m_renderCommandList, m_renderTextures[0]->texture.buffer);
    Transition(m_renderCommandList, m_renderTextures[0]->texture.buffer, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    ExecCommandList(m_renderCommandList);

    // Apply masked animations for first-person
    m_game->BeforeMainRender(*this);
    for (MaterialData& material : m_materials)
    {
        for (EntityData* entityData : material.entities)
        {
            entityData->firstPersonBoneConstantBuffer.UploadData(m_frameIndex);
        }
    }

    // 'Main' render
    ThrowIfFailed(m_renderCommandList->Reset(m_renderCommandAllocators[m_frameIndex], nullptr));
    Transition(m_renderCommandList, renderTargetWindow, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    m_renderCommandList->ClearRenderTargetView(mainRtvHandle, m_game->GetClearColor(), 0, nullptr);
    m_renderCommandList->ClearDepthStencilView(mainDsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    m_renderCommandList->RSSetViewports(1, &m_viewport);
    m_renderCommandList->RSSetScissorRects(1, &m_scissorRect);

    RenderScene(m_renderCommandList, mainRtvHandle, mainDsvHandle, mainCamera);
    RenderWireframe(m_renderCommandList, mainRtvHandle, mainDsvHandle, mainCamera);
    RenderDebugLines(m_renderCommandList, mainRtvHandle, mainDsvHandle, mainCamera);
    
    if (m_msaaEnabled)
    {
        Transition(m_renderCommandList, renderTargetMsaa, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_SOURCE);
        Transition(m_renderCommandList, renderTargetWindow, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_RESOLVE_DEST);
        m_renderCommandList->ResolveSubresource(renderTargetWindow, 0, renderTargetMsaa, 0, DISPLAY_FORMAT);
        Transition(m_renderCommandList, renderTargetMsaa, D3D12_RESOURCE_STATE_RESOLVE_SOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET);
        Transition(m_renderCommandList, renderTargetWindow, D3D12_RESOURCE_STATE_RESOLVE_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
    }

    DrawImgui(m_renderCommandList, &m_swapchainRtvHandles[m_frameIndex]);

    Transition(m_renderCommandList, renderTargetWindow, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    ExecCommandList(m_renderCommandList);
    EndProfile("Render Main");
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

    Transition(renderList, m_shadowmap->textureResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    renderList->OMSetRenderTargets(0, nullptr, FALSE, &m_shadowmap->depthStencilViewCPU);

    // Record commands.
    renderList->ClearDepthStencilView(m_shadowmap->depthStencilViewCPU, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    renderList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (MaterialData& data : m_materials)
    {
        for (EntityData* entity : data.entities)
        {
            if (!entity->visible || entity->wireframe) continue;
            renderList->IASetVertexBuffers(0, 1, &entity->vertexBufferView);
            renderList->SetGraphicsRootDescriptorTable(ENTITY, entity->constantBuffer.handles[m_frameIndex].gpuHandle);
            renderList->SetGraphicsRootDescriptorTable(BONES, entity->boneConstantBuffer.handles[m_frameIndex].gpuHandle);
            renderList->DrawInstanced(entity->vertexBufferView.SizeInBytes / entity->vertexBufferView.StrideInBytes, 1, 0, 0);
        }
    }

    Transition(renderList, m_shadowmap->textureResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

void EngineCore::RenderScene(ID3D12GraphicsCommandList* renderList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, CameraData* camera)
{
    renderList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap };
    renderList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Record commands.
    renderList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (MaterialData& data : m_materials)
    {
        renderList->SetPipelineState(data.pipeline->pipelineState);
        renderList->SetGraphicsRootSignature(data.pipeline->rootSignature);

        renderList->SetGraphicsRootDescriptorTable(SCENE, m_sceneConstantBuffer.handles[m_frameIndex].gpuHandle);
        renderList->SetGraphicsRootDescriptorTable(LIGHT, m_lightConstantBuffer.handles[m_frameIndex].gpuHandle);
        renderList->SetGraphicsRootDescriptorTable(CAMERA, camera->constantBuffer.handles[m_frameIndex].gpuHandle);
        renderList->SetGraphicsRootDescriptorTable(SHADOWMAP, m_shadowmap->shaderResourceViewHandle.gpuHandle);

        bool skipRender = false;
        for (int textureIdx = 0; textureIdx < data.pipeline->textureSlotCount; textureIdx++)
        {
            bool isRenderTexture = false;
            for (RenderTexture* rt : m_renderTextures)
				if (&rt->texture == data.textures[textureIdx])
					isRenderTexture = true;

            if (!camera->skipRenderTextures || !isRenderTexture)
            {
                assert(data.pipeline->textureSlotCount == data.textures.size);
                renderList->SetGraphicsRootDescriptorTable(CUSTOM_START + textureIdx, data.textures[textureIdx]->handle.gpuHandle);
            }
            else
            {
                skipRender = true;
            }
        }

        if (skipRender) continue;

        for (EntityData* entity : data.entities)
        {
            if (!entity->visible || entity->wireframe) continue;
            if (entity->mainOnly && camera != mainCamera) continue;
            renderList->IASetVertexBuffers(0, 1, &entity->vertexBufferView);
            renderList->SetGraphicsRootDescriptorTable(ENTITY, entity->constantBuffer.handles[m_frameIndex].gpuHandle);
            auto& boneBuffer = camera == mainCamera ? entity->firstPersonBoneConstantBuffer : entity->boneConstantBuffer;
            renderList->SetGraphicsRootDescriptorTable(BONES, boneBuffer.handles[m_frameIndex].gpuHandle);
            renderList->DrawInstanced(entity->vertexBufferView.SizeInBytes / entity->vertexBufferView.StrideInBytes, 1, 0, 0);
        }
    }
}

void EngineCore::RenderWireframe(ID3D12GraphicsCommandList* renderList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, CameraData* camera)
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
    renderList->SetGraphicsRootDescriptorTable(CAMERA, camera->constantBuffer.handles[m_frameIndex].gpuHandle);

    renderList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Record commands.
    renderList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    renderList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (MaterialData& data : m_materials)
    {
        for (EntityData* entity : data.entities)
        {
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

void EngineCore::RenderDebugLines(ID3D12GraphicsCommandList* renderList, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle, CameraData* camera)
{
    // Set necessary state.
    renderList->SetPipelineState(m_debugLineConfig->pipelineState);
    renderList->SetGraphicsRootSignature(m_debugLineConfig->rootSignature);
    renderList->RSSetViewports(1, &m_viewport);
    renderList->RSSetScissorRects(1, &m_scissorRect);

    // Load heaps
    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap };
    renderList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    renderList->SetGraphicsRootDescriptorTable(SCENE, m_sceneConstantBuffer.handles[m_frameIndex].gpuHandle);
    renderList->SetGraphicsRootDescriptorTable(CAMERA, camera->constantBuffer.handles[m_frameIndex].gpuHandle);

    renderList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Upload lines for current frame to lineVertexBuffer
    UINT8* pVertexDataBegin = nullptr;
    CD3DX12_RANGE readRange(0, 0);
    CHECK_HRCMD(m_debugLineData.vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, m_debugLineData.lineVertices.base, m_debugLineData.lineVertices.size * sizeof(Vertex));
    m_debugLineData.vertexBuffer->Unmap(0, nullptr);

    // Record commands.
    renderList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
	renderList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    renderList->IASetVertexBuffers(0, 1, &m_debugLineData.vertexBufferView);
	renderList->DrawInstanced(m_debugLineData.lineVertices.size, 1, 0, 0);
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

void EngineCore::OnShaderReload()
{
    WaitForGpu();

    std::wstring_convert<std::codecvt_utf8<wchar_t>> utf8_conv;

    for (MaterialData& material : m_materials)
    {
        CreatePipelineState(material.pipeline, true);

        if (FAILED(material.pipeline->creationError))
        {
            _com_error err(material.pipeline->creationError);

            m_shaderError.append("Failed to create material pipeline state: ");
            m_shaderError.append(utf8_conv.to_bytes(err.ErrorMessage()));
            return;
        }
    }

    CreatePipelineState(m_shadowConfig, true);
    if (FAILED(m_shadowConfig->creationError))
    {
        _com_error err(m_shadowConfig->creationError);

        m_shaderError.append("Failed to create shadow pipeline state: ");
        m_shaderError.append(utf8_conv.to_bytes(err.ErrorMessage()));
        return;
    }

    CreatePipelineState(m_wireframeConfig, true);
    if (FAILED(m_wireframeConfig->creationError))
    {
        _com_error err(m_wireframeConfig->creationError);

        m_shaderError.append("Failed to create wireframe pipeline state: ");
        m_shaderError.append(utf8_conv.to_bytes(err.ErrorMessage()));
        return;
    }

    CreatePipelineState(m_debugLineConfig, true);
    if (FAILED(m_debugLineConfig->creationError))
    {
        _com_error err(m_debugLineConfig->creationError);

        m_shaderError.append("Failed to create debug line pipeline state: ");
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

    comPointersLevel.Clear();
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
