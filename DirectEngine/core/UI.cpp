#include "UI.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "../Helpers.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <chrono>

bool initialized = false;

ID3D12DescriptorHeap* g_pd3dSrvDescHeap = NULL;

void SetupImgui(HWND hwnd, EngineCore* engine, int framesInFlight)
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();

	// DX12 descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NumDescriptors = 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(engine->m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap)));

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX12_Init(engine->m_device.Get(), framesInFlight, EngineCore::DisplayFormat, g_pd3dSrvDescHeap, g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(), g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

	// Load Fonts
	io.Fonts->AddFontFromFileTTF("Montserrat-Regular.ttf", 16.0f);

	static const ImWchar icons_ranges[] = { 0xEA01, 0xF2DF, 0 };
	ImFontConfig iconsConfig;
	iconsConfig.MergeMode = true;
	iconsConfig.PixelSnapH = true;
	iconsConfig.GlyphOffset = ImVec2{ 0, 4 };
	io.Fonts->AddFontFromFileTTF("remixicon.ttf", 16.0f, &iconsConfig, icons_ranges);

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

void DestroyImgui()
{
	if (g_pd3dSrvDescHeap != nullptr)
	{
		g_pd3dSrvDescHeap->Release();
		g_pd3dSrvDescHeap = nullptr;
	}
}