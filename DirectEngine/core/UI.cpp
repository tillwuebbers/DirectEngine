#include "UI.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"

#include "remixicon.h"
#include "../Helpers.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <chrono>
#include "../imgui/ImGuiProfilerRenderer.h"

bool initialized = false;
bool showDemoWindow = false;
bool debugVisible = true;

ID3D12DescriptorHeap* g_pd3dSrvDescHeap = NULL;
ImGuiUtils::ProfilersWindow* window;

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

	window = new ImGuiUtils::ProfilersWindow();
	
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

	if (showDemoWindow) ImGui::ShowDemoWindow(&showDemoWindow);

	if (debugVisible)
	{
		std::string title(ICON_BUG_FILL);
		title.append(" Debug");

		ImGui::SetNextWindowPos(ImVec2{ 0, 0 });
		if (ImGui::Begin(title.c_str(), &debugVisible, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
		{
			switch (engine->m_windowMode)
			{
			case WindowMode::Borderless:
				ImGui::Text("Window mode: Borderless");
				break;
			case WindowMode::Fullscreen:
				ImGui::Text("Window mode: Fullscreen");
				break;
			case WindowMode::Windowed:
				ImGui::Text("Window mode: Window");
				break;
			}

			ImGui::NewLine();

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			const float maxDisplayedFrameTime = (1.f / 144.f) * 2.f;
			const float lineWidth = 4.f;
			ImVec2 pos = ImGui::GetCursorPos();
			ImVec2 max = ImVec2(ImGui::GetWindowContentRegionMax().x, pos.y + 80);
			int rectWidth = static_cast<int>(ImGui::GetWindowContentRegionWidth());
			float rectHeight = max.y - pos.y;

			drawList->AddRectFilled(pos, max, ImColor::HSV(0, 0, 0), 3);
			drawList->AddLine(ImVec2(pos.x, pos.y + rectHeight / 2.f), ImVec2(max.x, pos.y + rectHeight / 2.f), ImColor::HSV(0, 0, .5f));

			int drawIndex = static_cast<int>((rectWidth - 1) / lineWidth);
			while (drawIndex >= 0)
			{
				int frameIndex = engine->m_lastDebugFrameIndex - drawIndex;
				while (frameIndex < 0) frameIndex += _ARRAYSIZE(engine->m_lastFrames);

				float linePercentHeight = engine->m_lastFrames[frameIndex].duration / maxDisplayedFrameTime;
				float p1Y = pos.y + rectHeight * (1.f - linePercentHeight);
				drawList->AddLine(ImVec2(pos.x + drawIndex * lineWidth, p1Y), ImVec2(pos.x + drawIndex * lineWidth, max.y), ImColor(0.f, 1.f, 0.f), lineWidth);

				drawIndex--;
			}

			if (ImGui::Button(engine->m_pauseDebugFrames ? ICON_PLAY_FILL : ICON_PAUSE_FILL))
			{
				engine->m_pauseDebugFrames = !engine->m_pauseDebugFrames;
			}

			ImGui::SetCursorPos(max);

			ImGui::NewLine();

			if (ImGui::Checkbox("Borderless", &engine->m_wantBorderless))
			{
				if (engine->m_windowMode == WindowMode::Fullscreen)
				{
					engine->m_wantedWindowMode = WindowMode::Borderless;
				}
				else if (engine->m_windowMode == WindowMode::Borderless)
				{
					engine->m_wantedWindowMode = WindowMode::Fullscreen;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Toggle Fullscreen"))
			{
				engine->ToggleWindowMode();
			}
			if (ImGui::Checkbox("VSync", &engine->m_useVsync))
			{

			}

			ImGui::NewLine();

			if (ImGui::Button("ToggleDemo"))
			{
				showDemoWindow = !showDemoWindow;
			}
			ImGui::SameLine();
			if (ImGui::Button("Show Log"))
			{
				//engine->m_game->showLog = true;
			}
		}
		ImGui::End();

		if (!engine->m_pauseDebugFrames)
		{
			window->cpuGraph.LoadFrameData(engine->m_profilerTaskData.data(), engine->m_profilerTaskData.size());
			engine->m_profilerTaskData.clear();
			engine->m_profilerTasks.clear();
		}
		window->Render();
	}

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