#include "UI.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "../Helpers.h"
#include "Constants.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <chrono>

bool initialized = false;

ID3D12DescriptorHeap* g_pd3dSrvDescHeap = NULL;

Texture g_debugTexture{};

void SetupImgui(HWND hwnd, EngineCore* engine, int framesInFlight)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	engine->m_imguiContext = ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGuiStyle& style = ImGui::GetStyle();
	ImGui::StyleColorsDark();

	// DX12 descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 2;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(engine->m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)));

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(engine->m_device, framesInFlight, DISPLAY_FORMAT, g_pd3dSrvDescHeap, g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(), g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

	// Create debug texture
	UINT increment = engine->m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	g_debugTexture.handle.cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(), 1, increment);
	g_debugTexture.handle.gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart(), 1, increment);

	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = 1024;
	texDesc.Height = 2048;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 12;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(engine->m_device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, NULL, NewComObject(engine->comPointers, &g_debugTexture.buffer)));
	g_debugTexture.buffer->SetName(L"Imgui debug texture");

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = texDesc.MipLevels;
	engine->m_device->CreateShaderResourceView(g_debugTexture.buffer, &srvDesc, g_debugTexture.handle.cpuHandle);

	// Load Fonts
	io.Fonts->AddFontFromFileTTF("Montserrat-Regular.ttf", 16.0f);

	static const ImWchar icons_ranges[] = { 0xEA01, 0xF2DF, 0 };
	ImFontConfig iconsConfig;
	iconsConfig.MergeMode = true;
	iconsConfig.PixelSnapH = true;
	iconsConfig.GlyphOffset = ImVec2{ 0, 4 };
	io.Fonts->AddFontFromFileTTF("remixicon.ttf", 16.0f, &iconsConfig, icons_ranges);

	// Set flags
	io.ConfigWindowsMoveFromTitleBarOnly = true;
	style.FrameRounding = 2.f;

	initialized = true;
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImGuiInputBlock ParseImGuiInputs(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	auto result = ImGuiInputBlock{};
	result.blockKeyboard = false;
	result.blockMouse = false;

	if (!initialized) return result;

	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);

	ImGuiIO io = ImGui::GetIO();
	result.blockKeyboard = io.WantCaptureKeyboard;
	result.blockMouse = io.WantCaptureMouse;
	return result;
}

void NewImguiFrame()
{
	if (!initialized) return;

	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void UpdateImgui(EngineCore* engine)
{
	if (!initialized) return;

	if (!engine->m_shaderError.empty())
	{
		bool staysOpen = true;
		ImGui::Begin("Shader Error", &staysOpen);
		ImGui::Text(engine->m_shaderError.c_str());
		ImGui::End();

		if (!staysOpen)
		{
			engine->m_shaderError.clear();
		}
	}
}

void DrawImgui(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE* renderTargetDescriptorView)
{
	if (!initialized) return;

	ImGui::Render();
	commandList->OMSetRenderTargets(1, renderTargetDescriptorView, FALSE, NULL);
	commandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}

void CopyDebugImage(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource)
{
	CD3DX12_RESOURCE_BARRIER barrierWrite = CD3DX12_RESOURCE_BARRIER::Transition(g_debugTexture.buffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	commandList->ResourceBarrier(1, &barrierWrite);

	commandList->CopyResource(g_debugTexture.buffer, resource);

	CD3DX12_RESOURCE_BARRIER barrierDraw = CD3DX12_RESOURCE_BARRIER::Transition(g_debugTexture.buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	commandList->ResourceBarrier(1, &barrierDraw);
}

void DrawDebugImage(ImVec2 size)
{
	ImGui::Image((ImTextureID)g_debugTexture.handle.gpuHandle.ptr, size);
}

void DestroyImgui()
{
	if (g_pd3dSrvDescHeap != nullptr)
	{
		g_pd3dSrvDescHeap->Release();
		g_pd3dSrvDescHeap = nullptr;
	}
	ImGui_ImplDX12_Shutdown();
}