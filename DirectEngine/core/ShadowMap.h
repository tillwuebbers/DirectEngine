#include <d3d12.h>
#include <d3dx12.h>

#include "Common.h"
#include "ComStack.h"

struct ShadowMap
{
	UINT width;
	UINT height;
	CD3DX12_VIEWPORT viewport;
	CD3DX12_RECT scissorRect;
	ID3D12Resource* textureResource;
	DescriptorHandle shaderResourceViewHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE depthStencilViewCPU;
	DXGI_FORMAT format;

	ShadowMap(DXGI_FORMAT format);

	void SetSize(UINT newWidth, UINT newHeight);
	void Build(ID3D12Device* device, ComStack& comStack);
};

struct RenderTexture
{
	UINT width;
	UINT height;
	DXGI_FORMAT format;
	CD3DX12_VIEWPORT viewport;
	CD3DX12_RECT scissorRect;

	Texture texture{};
	ID3D12Resource* msaaBuffer;
	ID3D12Resource* dsvBuffer;
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle{};
	CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle{};
};