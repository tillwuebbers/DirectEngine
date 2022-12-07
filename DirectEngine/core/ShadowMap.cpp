#include "ShadowMap.h"
#include "../Helpers.h"

ShadowMap::ShadowMap(DXGI_FORMAT format) : format(format)
{
}

void ShadowMap::SetSize(UINT newWidth, UINT newHeight)
{
	width = newWidth;
	height = newHeight;
	viewport = CD3DX12_VIEWPORT{ 0.f, 0.f, static_cast<float>(width), static_cast<float>(height) };
	scissorRect = CD3DX12_RECT{ 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
}

void ShadowMap::Build(ID3D12Device* device, ComStack& comStack)
{
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1; // Important! Will create very strange bugs if != 1
	texDesc.Format = format;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = DEPTH_BUFFER_FORMAT;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	auto heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &optClear, NewComObject(comStack, &textureResource)));
	textureResource->SetName(L"Shadowmap Texture");

	// Shader resource view for sampling
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Texture2D.PlaneSlice = 0;
	device->CreateShaderResourceView(textureResource, &srvDesc, shaderResourceViewHandle.cpuHandle);

	// Depth stencil view for depth buffer write
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = DEPTH_BUFFER_FORMAT;
	dsvDesc.Texture2D.MipSlice = 0;
	device->CreateDepthStencilView(textureResource, &dsvDesc, depthStencilViewCPU);
}
