#include "UI.h"

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "../Helpers.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <chrono>

void ImGuiUI::SetupImgui(HWND hwnd, ID3D12Device* device, int framesInFlight)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	imGuiContext = ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGuiStyle& style = ImGui::GetStyle();
	ImGui::StyleColorsDark();

	// DX12 descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = MAX_DEBUG_TEXTURES + 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)));
	g_pd3dSrvDescHeap->SetName(L"ImGui descriptor heap");

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(device, framesInFlight, DISPLAY_FORMAT, g_pd3dSrvDescHeap, g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(), g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

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

void ImGuiUI::NewImguiFrame()
{
	if (!initialized) return;

	// Start the Dear ImGui frame
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void ImGuiUI::DrawImgui(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE* renderTargetDescriptorView)
{
	if (!initialized) return;

	ImGui::Render();
	commandList->OMSetRenderTargets(1, renderTargetDescriptorView, FALSE, NULL);
	commandList->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
	g_frameTextureCount = 0;
}

void ImGuiUI::UIImage(ID3D12Device* device, TextureGPU* texture, ImVec2 size)
{
	if (!initialized) return;

	assert(g_frameTextureCount < MAX_DEBUG_TEXTURES);
	if (g_frameTextureCount >= MAX_DEBUG_TEXTURES) return;

	D3D12_RESOURCE_DESC textureDesc = texture->buffer->GetDesc();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = textureDesc.Format;
	srvDesc.Texture2D.MipLevels = textureDesc.MipLevels;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

	UINT increment = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	auto cpuDescriptor = CD3DX12_CPU_DESCRIPTOR_HANDLE(g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(), g_frameTextureCount + 1, increment);
	auto gpuDescriptor = CD3DX12_GPU_DESCRIPTOR_HANDLE(g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart(), g_frameTextureCount + 1, increment);
	g_frameTextureCount++;

	device->CreateShaderResourceView(texture->buffer, &srvDesc, cpuDescriptor);
	ImGui::Image(reinterpret_cast<ImTextureID>(gpuDescriptor.ptr), size);
}

void ImGuiUI::DestroyImgui()
{
	if (g_pd3dSrvDescHeap != nullptr)
	{
		g_pd3dSrvDescHeap->Release();
		g_pd3dSrvDescHeap = nullptr;
	}
	ImGui_ImplDX12_Shutdown();
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImGuiInputBlock ParseImGuiInputs(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui::GetCurrentContext() == nullptr) return ImGuiInputBlock{};

	auto result = ImGuiInputBlock{};
	result.blockKeyboard = false;
	result.blockMouse = false;

	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);

	ImGuiIO io = ImGui::GetIO();
	result.blockKeyboard = io.WantCaptureKeyboard;
	result.blockMouse = io.WantCaptureMouse;
	return result;
}
