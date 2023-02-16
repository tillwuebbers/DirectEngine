#include "Entity.h"

void Entity::AddChild(Entity* child)
{
	assert(child != nullptr);
	assert(childCount < MAX_ENTITY_CHILDREN);
	
	children[childCount] = child;
	childCount++;

	child->parent = this;
}

void Entity::RemoveChild(Entity* child)
{
	assert(child != nullptr);
	assert(child->parent == this);
	child->parent = nullptr;

	for (size_t i = 0; i < childCount; i++)
	{
		if (children[i] == child)
		{
			if (childCount > 1)
			{
				children[i] = children[childCount - 1];
			}
			childCount--;
			break;
		}
	}
}

EntityData& Entity::GetData()
{
	assert(isRendered);
	return *engine->m_materials[materialIndex].entities[dataIndex];
}

EntityConstantBuffer& Entity::GetBuffer()
{
	assert(isRendered);
	return GetData().constantBuffer.data;
}

void Entity::UpdateWorldMatrix()
{
	// TODO: we're duplicating state here, bad
	localMatrix = DirectX::XMMatrixAffineTransformation(scale, XMVECTOR{}, rotation, position);

	if (parent == nullptr)
	{
		worldMatrix = localMatrix;
	}
	else
	{
		MAT_RMAJ parentTransform = DirectX::XMMatrixAffineTransformation(parent->scale, XMVECTOR{}, parent->rotation, parent->position);
		worldMatrix = localMatrix * parent->worldMatrix;
	}

	if (isRendered)
	{
		GetBuffer().worldTransform = DirectX::XMMatrixTranspose(worldMatrix);
	}

	for (size_t i = 0; i < childCount; i++)
	{
		children[i]->UpdateWorldMatrix();
	}
}

XMVECTOR Entity::LocalToWorld(XMVECTOR localPosition)
{
	EntityConstantBuffer buffer = GetBuffer();
	return XMVector4Transform(localPosition, buffer.worldTransform);
}

XMVECTOR Entity::WorldToLocal(XMVECTOR worldPosition)
{
	XMVECTOR det;
	return XMVector4Transform(worldPosition, XMMatrixInverse(&det, GetBuffer().worldTransform));
}

void Entity::Disable()
{
	isActive = false;
	GetData().visible = false;
}