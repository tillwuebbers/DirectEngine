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
	ID3D12Resource* textureResource = nullptr;
	DescriptorHandle shaderResourceViewHandle;
	CD3DX12_CPU_DESCRIPTOR_HANDLE depthStencilViewCPU;

	ShadowMap(UINT width, UINT height);

	void Build(ID3D12Device* device, ComStack& comStack);
};
