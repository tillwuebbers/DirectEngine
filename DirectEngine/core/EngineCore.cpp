#pragma warning (disable : 4996)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
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
    CheckTearingSupport();
    const wchar_t* windowClassName = L"DX12WindowClass";
    RegisterWindowClass(hInst, windowClassName, wndProc);
    CreateGameWindow(windowClassName, hInst, m_width, m_height);

    LoadPipeline();
    LoadAssets();

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

// Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, *ppAdapter will be set to nullptr.
_Use_decl_annotations_
void EngineCore::GetHardwareAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        DXGI_GPU_PREFERENCE gpuPreference = requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED;
        for (UINT adapterIndex = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(adapterIndex, gpuPreference, IID_PPV_ARGS(&adapter))); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the sactual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    if (adapter.Get() == nullptr)
    {
        for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    *ppAdapter = adapter.Detach();
}

// Load the rendering pipeline dependencies.
void EngineCore::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

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

        ThrowIfFailed(D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter, true);

        ThrowIfFailed(D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
    }

    // Describe and create the command queue.
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
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

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(m_commandQueue.Get(), m_hwnd, &swapChainDesc, nullptr, nullptr, &swapChain));

    factory->MakeWindowAssociation(m_hwnd, DXGI_MWA_NO_ALT_ENTER);

    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_swapChain->SetMaximumFrameLatency(1);
    m_frameWaitableObject = m_swapChain->GetFrameLatencyWaitableObject();

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Create descriptor heaps.
    {
        // Describe and create a render target view (RTV) descriptor heap.
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
        m_rtvHeap->SetName(L"Render Target View Descriptor Heap");

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Describe and create a constant buffer view (CBV) descriptor heap.
        // Flags indicate that this descriptor heap can be bound to the pipeline 
        // and that descriptors contained in it can be referenced by a root table.
        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
        cbvHeapDesc.NumDescriptors = MAX_DESCRIPTORS;
        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
        m_cbvHeap->SetName(L"Constant Buffer View Descriptor Heap");

        // Depth/Stencil descriptor heap
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_depthStencilHeap)));
        m_depthStencilHeap->SetName(L"Depth/Stencil Descriptor Heap");

        // Shadowmap heap
        D3D12_DESCRIPTOR_HEAP_DESC shadowHeapDesc = {};
        shadowHeapDesc.NumDescriptors = 1;
        shadowHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        shadowHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&shadowHeapDesc, IID_PPV_ARGS(&m_shadowmapHeap)));
        m_shadowmapHeap->SetName(L"Shadowmap Descriptor Heap");
    }

    for (UINT n = 0; n < FrameCount; n++)
    {
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_uploadCommandAllocators[n])));
        m_uploadCommandAllocators[n]->SetName(std::format(L"Upload Command Allocator {}", n).c_str());
        ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_renderCommandAllocators[n])));
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

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            m_renderTargets[n]->SetName(std::format(L"Engine Render Target {}", n).c_str());
            rtvHandle.Offset(1, m_rtvDescriptorSize);
        }
    }

    // depth/stencil view
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
        depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        CD3DX12_HEAP_PROPERTIES heapType(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC depthTexture = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        ThrowIfFailed(m_device->CreateCommittedResource(&heapType, D3D12_HEAP_FLAG_NONE, &depthTexture, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue, IID_PPV_ARGS(&m_depthStencilBuffer)));
        m_depthStencilBuffer->SetName(L"Depth/Stencil Buffer");
        m_device->CreateDepthStencilView(m_depthStencilBuffer.Get(), &depthStencilDesc, m_depthStencilHeap->GetCPUDescriptorHandleForHeapStart());
    }

    // Shadowmap view
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
        depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        CD3DX12_HEAP_PROPERTIES heapType(D3D12_HEAP_TYPE_DEFAULT);
        CD3DX12_RESOURCE_DESC shadowTexture = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        ThrowIfFailed(m_device->CreateCommittedResource(&heapType, D3D12_HEAP_FLAG_NONE, &shadowTexture, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue, IID_PPV_ARGS(&m_shadowmapBuffer)));
        m_shadowmapBuffer->SetName(L"Shadowmap Buffer");
        m_device->CreateDepthStencilView(m_shadowmapBuffer.Get(), &depthStencilDesc, m_shadowmapHeap->GetCPUDescriptorHandleForHeapStart());
    }
}

HRESULT EngineCore::CreatePipelineState(const char* vsEntryName, const char* psEntryName, const wchar_t* debugName, ID3D12PipelineState** outState)
{
    ComPtr<ID3DBlob> vertexShader;
    ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
    const wchar_t* shaderPath = L"../../../../DirectEngine/shaders/shaders.hlsl";

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
    const wchar_t* shaderPath = L"shaders.hlsl";
#endif

    ComPtr<ID3DBlob> vsErrors;
    ComPtr<ID3D10Blob> psErrors;
    HRESULT hr;

    m_shaderError.clear();
    hr = D3DCompileFromFile(shaderPath, nullptr, nullptr, vsEntryName, "vs_5_0", compileFlags, 0, &vertexShader, &vsErrors);
    if (vsErrors)
    {
        m_shaderError = std::format("Vertex Shader Errors:\n{}\n", (LPCSTR)vsErrors->GetBufferPointer());
        OutputDebugStringA(m_shaderError.c_str());
    }
    if (FAILED(hr))
    {
        return hr;
    }

    hr = D3DCompileFromFile(shaderPath, nullptr, nullptr, psEntryName, "ps_5_0", compileFlags, 0, &pixelShader, &psErrors);
    if (psErrors)
    {
        m_shaderError = std::format("Pixel Shader Errors:\n{}\n", (LPCSTR)psErrors->GetBufferPointer());
        OutputDebugStringA(m_shaderError.c_str());
    }
    if (FAILED(hr))
    {
        return hr;
    }

    // Define the vertex input layout.
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
    psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(outState));
    m_pipelineState->SetName(debugName);
    return hr;
}

void EngineCore::LoadAssets()
{
    // Create a root signature consisting of a descriptor table with a single CBV.
    {
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};

        // This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
        CD3DX12_ROOT_PARAMETER1 rootParameters[3];

        ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC); // scene CBV
        ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC); // light CBV
        ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC); // entity CBV
        rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_ALL);
        rootParameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_VERTEX);

        // Allow input layout and deny uneccessary access to certain pipeline stages.
        D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc{};
        rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        if (FAILED(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error)))
        {
            OutputDebugStringA(reinterpret_cast<const char*>(error->GetBufferPointer()));
        }
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
        m_rootSignature->SetName(L"Engine Root Signature");
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    ThrowIfFailed(CreatePipelineState("VSMain", "PSMain", L"Main Material Pipeline", &m_pipelineState));
    ThrowIfFailed(CreatePipelineState("VSShadow", "PSShadow", L"Shadow Pipeline", &m_shadowPipelineState));

    // Create the render command list
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_renderCommandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_renderCommandList.list)));
    ThrowIfFailed(m_renderCommandList.list->Close());
    m_renderCommandList.list->SetName(L"Render Command List");

    // Create the upload command list and use it
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_uploadCommandAllocators[m_frameIndex].Get(), nullptr, IID_PPV_ARGS(&m_uploadCommandList.list)));
    ThrowIfFailed(m_uploadCommandList.list->Close());
    m_uploadCommandList.list->SetName(L"Upload Command List");

    // Shader values for scene
    CreateConstantBuffers<SceneConstantBuffer>(m_sceneConstantBuffers, m_mappedSceneData);
    CreateConstantBuffers<LightConstantBuffer>(m_lightConstantBuffers, m_mappedLightData);
    for (int i = 0; i < FrameCount; i++)
    {
        memcpy(m_mappedSceneData[i], &m_sceneData, sizeof(m_sceneData));
        m_sceneConstantBuffers[i]->SetName(std::format(L"Scene Constant Buffer {}", i).c_str());

        memcpy(m_mappedLightData[i], &m_lightData, sizeof(m_lightData));
        m_lightConstantBuffers[i]->SetName(std::format(L"Light Constant Buffer {}", i).c_str());
    }

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
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

size_t EngineCore::CreateMaterial(const size_t maxVertices, const size_t vertexStride)
{
    const size_t maxByteCount = vertexStride * maxVertices;

    MaterialData& data = m_materials.emplace_back();
    data.maxVertexCount = maxVertices;
    size_t dataIndex = m_materials.size() - 1;
    data.vertexStride = vertexStride;

    // Create buffer for upload
    CD3DX12_HEAP_PROPERTIES tempHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC tempBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(maxByteCount);
    ThrowIfFailed(m_device->CreateCommittedResource(&tempHeapProperties, D3D12_HEAP_FLAG_NONE, &tempBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&data.vertexUploadBuffer)));
    data.vertexUploadBuffer->SetName(L"Vertex Upload Buffer");

    // Create real vertex buffer on gpu memory
    CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(maxByteCount);
    ThrowIfFailed(m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&data.vertexBuffer)));
    data.vertexBuffer->SetName(L"Vertex Buffer");

    return dataIndex;
}

D3D12_VERTEX_BUFFER_VIEW EngineCore::CreateMesh(const size_t materialIndex, const void* vertexData, const size_t vertexCount)
{
    // Update draw call
    MaterialData& data = m_materials[materialIndex];
    const size_t sizeInBytes = data.vertexStride * vertexCount;
    const size_t offsetInBuffer = data.vertexCount * data.vertexStride;

    D3D12_VERTEX_BUFFER_VIEW view{};
    view.BufferLocation = data.vertexBuffer->GetGPUVirtualAddress() + offsetInBuffer;
    view.StrideInBytes = data.vertexStride;
    view.SizeInBytes = sizeInBytes;

    assert(data.vertexCount + vertexCount <= data.maxVertexCount);

    // Upload to temp buffer
    UINT8* pVertexDataBegin = nullptr;
    CD3DX12_RANGE readRange(0, 0);
    ThrowIfFailed(data.vertexUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin + offsetInBuffer, vertexData, sizeInBytes);
    data.vertexUploadBuffer->Unmap(0, nullptr);

    data.vertexCount += vertexCount;
    return view;
}

size_t EngineCore::CreateEntity(const size_t materialIndex, D3D12_VERTEX_BUFFER_VIEW& meshView)
{
    EntityData& entity = m_entities.emplace_back();
    entity.entityIndex = m_entities.size() - 1;
    entity.materialIndex = materialIndex;
    entity.vertexBufferView = meshView;

    CreateConstantBuffers<EntityConstantBuffer>(entity.constantBuffers, entity.mappedConstantBufferData);
    for (int i = 0; i < FrameCount; i++)
    {
        entity.constantBuffers[i]->SetName(std::format(L"Entity {} Constant Buffer {}", entity.entityIndex, i).c_str());
    }

    return entity.entityIndex;
}

void EngineCore::FinishMaterialSetup(size_t materialIndex)
{
    MaterialData& data = m_materials[materialIndex];

    // Copy data to vertex buffer
    m_uploadCommandList.list->CopyResource(data.vertexBuffer.Get(), data.vertexUploadBuffer.Get());
    CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(data.vertexBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    m_uploadCommandList.list->ResourceBarrier(1, &transition);

    ScheduleCommandList(&m_uploadCommandList);
}

void EngineCore::OnUpdate()
{
    std::chrono::steady_clock::time_point now = std::chrono::high_resolution_clock::now();
    double secondsSinceStart = NanosecondsToSeconds(now - m_startTime);
    m_updateDeltaTime = NanosecondsToSeconds(now - m_frameStartTime);
    m_frameStartTime = now;

    m_sceneData.time = static_cast<float>(secondsSinceStart);
    m_sceneData.deltaTime = static_cast<float>(m_updateDeltaTime);

    NewImguiFrame();
    UpdateImgui(this);

    BeginProfile("Reset Upload CB", ImColor::HSV(.33f, .33f, 1.f));
    ThrowIfFailed(m_uploadCommandAllocators[m_frameIndex]->Reset());
    m_uploadCommandList.Reset(m_uploadCommandAllocators[m_frameIndex].Get(), m_pipelineState.Get());
    EndProfile("Reset Upload CB");

    if (!m_gameStarted)
    {
        BeginProfile("Start Game", ImColor::HSV(0.f, 0.f, 1.f));
        m_gameStarted = true;
        m_game->StartGame(*this);
        EndProfile("Start Game");
    }

    m_game->UpdateGame(*this);

    BeginProfile("Finish Update", ImColor::HSV(.2f, .33f, 1.f));
    ThrowIfFailed(m_uploadCommandList.list->Close());
    EndProfile("Finish Update");
}

void EngineCore::OnRender()
{
    BeginProfile("Commands", ImColor::HSV(.0, .5, 1.));
    double test = TimeSinceStart();

    if (m_wantedWindowMode != m_windowMode)
    {
        ApplyWindowMode();
    }

    PopulateCommandList();

    std::vector<ID3D12CommandList*> lists{};
    for (CommandList* list : m_scheduledCommandLists)
    {
        list->scheduled = false;
        lists.push_back(list->list.Get());
    }
    m_commandQueue->ExecuteCommandLists(lists.size(), lists.data());
    m_scheduledCommandLists.clear();
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

void EngineCore::OnDestroy()
{
    WaitForGpu();
    m_wantedWindowMode = WindowMode::Windowed;
    ApplyWindowMode();
    CloseHandle(m_fenceEvent);
}

void EngineCore::ScheduleCommandList(CommandList* newList)
{
    newList->scheduled = true;
    for (CommandList* list : m_scheduledCommandLists)
    {
        if (newList == list) return;
    }
    m_scheduledCommandLists.push_back(newList);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE* EngineCore::GetConstantBufferHandle(int rootSigDescriptorOffset)
{
    UINT offset = rootSigDescriptorOffset * FrameCount + m_frameIndex;
    CD3DX12_GPU_DESCRIPTOR_HANDLE* descriptor = NewObject(frameArena, CD3DX12_GPU_DESCRIPTOR_HANDLE);
    D3D12_GPU_DESCRIPTOR_HANDLE start = m_cbvHeap->GetGPUDescriptorHandleForHeapStart();
    descriptor->InitOffsetted(start, offset, m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    return descriptor;
}

void EngineCore::RenderShadows(ID3D12GraphicsCommandList* renderList)
{
    m_lightData.shadowPassActive = true;
    memcpy(m_mappedSceneData[m_frameIndex], &m_sceneData, sizeof(m_sceneData));
    memcpy(m_mappedLightData[m_frameIndex], &m_lightData, sizeof(m_lightData));

    // Set necessary state.
    renderList->SetPipelineState(m_shadowPipelineState.Get());
    renderList->SetGraphicsRootSignature(m_rootSignature.Get());
    renderList->RSSetViewports(1, &m_viewport);
    renderList->RSSetScissorRects(1, &m_scissorRect);

    // Constant buffer descriptor heap
    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };
    renderList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    renderList->SetGraphicsRootDescriptorTable(SCENE, *GetConstantBufferHandle(SCENE));
    renderList->SetGraphicsRootDescriptorTable(LIGHT, *GetConstantBufferHandle(LIGHT));

    // Barrier to transition shadow map?
    CD3DX12_CPU_DESCRIPTOR_HANDLE shadowHandle(m_shadowmapHeap->GetCPUDescriptorHandleForHeapStart());
    renderList->OMSetRenderTargets(0, nullptr, FALSE, &shadowHandle);

    // Record commands.
    renderList->ClearDepthStencilView(shadowHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    renderList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (int drawIdx = 0; drawIdx < m_materials.size(); drawIdx++)
    {
        MaterialData& data = m_materials[drawIdx];

        int entityIndex = 0;
        for (EntityData& entity : m_entities)
        {
            if (!entity.visible) continue;
            memcpy(entity.mappedConstantBufferData[m_frameIndex], &entity.constantBufferData, sizeof(EntityConstantBuffer));

            renderList->IASetVertexBuffers(0, 1, &entity.vertexBufferView);
            renderList->SetGraphicsRootDescriptorTable(RootSigBufferOffset::ENTITY, *GetConstantBufferHandle(ENTITY + entity.entityIndex));
            renderList->DrawInstanced(entity.vertexBufferView.SizeInBytes / entity.vertexBufferView.StrideInBytes, 1, 0, 0);

            entityIndex++;
        }
    }
}

void EngineCore::RenderScene(ID3D12GraphicsCommandList* renderList)
{
    m_lightData.shadowPassActive = false;
    memcpy(m_mappedSceneData[m_frameIndex], &m_sceneData, sizeof(m_sceneData));
    memcpy(m_mappedLightData[m_frameIndex], &m_lightData, sizeof(m_lightData));

    // Set necessary state.
    renderList->SetPipelineState(m_pipelineState.Get());
    renderList->SetGraphicsRootSignature(m_rootSignature.Get());
    renderList->RSSetViewports(1, &m_viewport);
    renderList->RSSetScissorRects(1, &m_scissorRect);

    // Constant buffer descriptor heap
    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };
    renderList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    renderList->SetGraphicsRootDescriptorTable(SCENE, *GetConstantBufferHandle(SCENE));
    renderList->SetGraphicsRootDescriptorTable(LIGHT, *GetConstantBufferHandle(LIGHT));

    // Indicate that the back buffer will be used as a render target.
    CD3DX12_RESOURCE_BARRIER barrierToRender = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    renderList->ResourceBarrier(1, &barrierToRender);

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_depthStencilHeap->GetCPUDescriptorHandleForHeapStart());
    renderList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    renderList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    renderList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    renderList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (int drawIdx = 0; drawIdx < m_materials.size(); drawIdx++)
    {
        MaterialData& data = m_materials[drawIdx];

        int entityIndex = 0;
        for (EntityData& entity : m_entities)
        {
            if (!entity.visible) continue;
            memcpy(entity.mappedConstantBufferData[m_frameIndex], &entity.constantBufferData, sizeof(EntityConstantBuffer));

            renderList->IASetVertexBuffers(0, 1, &entity.vertexBufferView);
            renderList->SetGraphicsRootDescriptorTable(ENTITY, *GetConstantBufferHandle(ENTITY + entity.entityIndex));
            renderList->DrawInstanced(entity.vertexBufferView.SizeInBytes / entity.vertexBufferView.StrideInBytes, 1, 0, 0);

            entityIndex++;
        }
    }

    // UI
    DrawImgui(renderList, &rtvHandle);
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
    m_renderCommandList.Reset(m_renderCommandAllocators[m_frameIndex].Get(), m_pipelineState.Get());
    ID3D12GraphicsCommandList* renderList = m_renderCommandList.list.Get();
    
    RenderShadows(renderList);
    RenderScene(renderList);

    // Indicate that the back buffer will now be used to present.
    CD3DX12_RESOURCE_BARRIER barrierToPresent = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    renderList->ResourceBarrier(1, &barrierToPresent);

    ThrowIfFailed(renderList->Close());

    ScheduleCommandList(&m_renderCommandList);
}

// Wait for pending GPU work to complete.
void EngineCore::WaitForGpu()
{
    // Schedule a Signal command in the queue.
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]));

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
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

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
    // Flush all current GPU commands.
    WaitForGpu();

    m_width = width;
    m_height = height;

    m_aspectRatio = (float)m_width / (float)m_height;
    m_viewport = CD3DX12_VIEWPORT{ 0.f, 0.f, static_cast<float>(m_width), static_cast<float>(m_height) };
    m_scissorRect = CD3DX12_RECT{ 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };

    // Release the resources holding references to the swap chain (requirement of
    // IDXGISwapChain::ResizeBuffers) and reset the frame fence values to the
    // current fence value.
    for (UINT n = 0; n < FrameCount; n++)
    {
        m_renderTargets[n].Reset();
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
    HRESULT hr = CreatePipelineState("VSMain", "PSMain", L"Main Material Pipeline", &m_pipelineState);

    if (FAILED(hr))
    {
        _com_error err(hr);

        m_shaderError.append("Failed to create material pipeline state: ");
        m_shaderError.append(utf8_conv.to_bytes(err.ErrorMessage()));
        return;
    }

    hr = CreatePipelineState("VSShadow", "PSShadow", L"Shadow Pipeline", &m_shadowPipelineState);
    if (FAILED(hr))
    {
        _com_error err(hr);

        m_shaderError.append("Failed to create shadow pipeline state: ");
        m_shaderError.append(utf8_conv.to_bytes(err.ErrorMessage()));
        return;
    }
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
        ThrowIfFailed(pOutput->GetDisplayModeList(DisplayFormat, 0, &numModes, modes));

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

inline double EngineCore::TimeSinceStart()
{
    return NanosecondsToSeconds(std::chrono::high_resolution_clock::now() - m_startTime);
}

inline double EngineCore::TimeSinceFrameStart()
{
    return NanosecondsToSeconds(std::chrono::high_resolution_clock::now() - m_frameStartTime);
}

void CommandList::Reset(ID3D12CommandAllocator* allocator, ID3D12PipelineState* pipelineState)
{
    assert(!scheduled);
    ThrowIfFailed(list->Reset(allocator, pipelineState));
}

inline double NanosecondsToSeconds(std::chrono::nanoseconds clock)
{
    return clock.count() / 1e+9;
}