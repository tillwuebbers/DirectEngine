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

void Entity::UpdateAudio(EngineCore& engine, const X3DAUDIO_LISTENER* audioListener)
{
	XMVECTOR entityForwards, entityRight, entityUp;
	CalculateDirectionVectors(entityForwards, entityRight, entityUp, rotation);

	IXAudio2SourceVoice* audioSourceVoice = audioSource.source;
	if (audioSourceVoice != nullptr)
	{
		X3DAUDIO_EMITTER& emitter = audioSource.audioEmitter;
		XMStoreFloat3(&emitter.OrientFront, entityForwards);
		XMStoreFloat3(&emitter.OrientTop, entityUp);
		XMStoreFloat3(&emitter.Position, position);
		XMStoreFloat3(&emitter.Velocity, velocity);

		XAUDIO2_VOICE_DETAILS audioSourceDetails;
		audioSourceVoice->GetVoiceDetails(&audioSourceDetails);

		X3DAUDIO_DSP_SETTINGS* dspSettings = emitter.ChannelCount == 1 ? &engine.m_audioDspSettingsMono : &engine.m_audioDspSettingsStereo;

		X3DAudioCalculate(engine.m_3daudio, audioListener, &audioSource.audioEmitter, X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER | X3DAUDIO_CALCULATE_LPF_DIRECT | X3DAUDIO_CALCULATE_REVERB, dspSettings);
		audioSourceVoice->SetOutputMatrix(engine.m_audioMasteringVoice, audioSourceDetails.InputChannels, engine.m_audioVoiceDetails.InputChannels, dspSettings->pMatrixCoefficients);
		audioSourceVoice->SetFrequencyRatio(dspSettings->DopplerFactor);
		// TODO: low pass filter + reverb
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

void CalculateDirectionVectors(XMVECTOR& outForward, XMVECTOR& outRight, XMVECTOR& outUp, XMVECTOR inRotation)
{
	XMMATRIX rotationMatrix = XMMatrixRotationQuaternion(inRotation);
	outForward = XMVector3Transform(V3_FORWARD, rotationMatrix);
	outRight = XMVector3Transform(V3_RIGHT, rotationMatrix);
	outUp = XMVector3Cross(outForward, outRight);
}