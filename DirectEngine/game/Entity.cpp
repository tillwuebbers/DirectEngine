#include "Entity.h"

EntityData* Entity::GetData()
{
	return &engine->m_materials[materialIndex].entities[dataIndex];
}

EntityConstantBuffer* Entity::GetBuffer()
{
	return &GetData()->constantBuffer.data;
}

XMVECTOR Entity::LocalToWorld(XMVECTOR localPosition)
{
	return XMVector4Transform(localPosition, GetBuffer()->worldTransform);
}

XMVECTOR Entity::WorldToLocal(XMVECTOR worldPosition)
{
	XMVECTOR det;
	return XMVector4Transform(worldPosition, XMMatrixInverse(&det, GetBuffer()->worldTransform));
}