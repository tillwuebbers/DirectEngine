#include <d3d12.h>
#include <d3dx12.h>

#include "Common.h"

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
using Microsoft::WRL::ComPtr;

struct ShadowMap
{
	UINT width;
	UINT height;
	CD3DX12_VIEWPORT viewport;
	CD3DX12_RECT scissorRect;
	ComPtr<ID3D12Resource> textureResource;
	DescriptorHandle shaderResourceViewHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE depthStencilViewCPU;
	DXGI_FORMAT format;

	ShadowMap(DXGI_FORMAT format);

	void SetSize(UINT newWidth, UINT newHeight);
	void Build(ID3D12Device* device);
};