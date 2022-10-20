#pragma once

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

#include "d3dx12.h"

#include "../core/EngineCore.h"

struct ImGuiInputBlock
{
	bool blockMouse;
	bool blockKeyboard;
};

void SetupImgui(HWND hwnd, ID3D12Device* device, int framesInFlight);
ImGuiInputBlock ParseImGuiInputs(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void NewImguiFrame();
void UpdateImgui(EngineCore* engine);
void DrawImgui(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE* renderTargetDescriptorView);