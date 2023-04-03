#pragma once

#include "Common.h"
#include "Constants.h"

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
#include "d3dx12.h"
#include "imgui.h"

struct ImGuiInputBlock
{
	bool blockMouse = false;
	bool blockKeyboard = false;
};

struct ImGuiTextureReference
{
	DescriptorHandle handle = {};
};

class ImGuiUI
{
public:
	void SetupImgui(HWND hwnd, ID3D12Device* device, int framesInFlight);
	void NewImguiFrame();
	void DrawImgui(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE* renderTargetDescriptorView);
	void UIImage(ID3D12Device* device, Texture* texture, ImVec2 size);
	void DestroyImgui();

	bool initialized = false;
	ImGuiContext* imGuiContext = nullptr;
	ID3D12DescriptorHeap* g_pd3dSrvDescHeap = NULL;
	ImGuiTextureReference g_imguiTextures[MAX_DEBUG_TEXTURES]{};
	size_t g_frameTextureCount = 0;
};

ImGuiInputBlock ParseImGuiInputs(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);