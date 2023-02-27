#include "Entity.h"

void Entity::AddChild(Entity* child)
{
	assert(child != nullptr);
	assert(child != this);
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

void Entity::UpdateAnimation(EngineCore& engine)
{
	if (isSkinnedRoot)
	{
		// Reset joints
		for (int jointIdx = 0; jointIdx < transformHierachy->nodeCount; jointIdx++)
		{
			TransformNode& node = transformHierachy->nodes[jointIdx];
			node.currentLocal = node.baseLocal;
		}

		// Apply animations
		for (int animIndex = 0; animIndex < transformHierachy->animationCount; animIndex++)
		{
			TransformAnimation& animation = transformHierachy->animations[animIndex];

			if (animation.active)
			{
				animation.time = fmodf(engine.TimeSinceStart(), animation.duration);

				for (int jointIdx = 0; jointIdx < transformHierachy->nodeCount; jointIdx++)
				{
					if (animation.activeChannels[jointIdx])
					{
						TransformNode& node = transformHierachy->nodes[jointIdx];

						XMVECTOR translation, rotation, scale;
						XMMatrixDecompose(&scale, &rotation, &translation, node.currentLocal);

						AnimationData& translationData = animation.jointChannels[jointIdx].translations;
						AnimationData& rotationData = animation.jointChannels[jointIdx].rotations;
						AnimationData& scaleData = animation.jointChannels[jointIdx].scales;

						if (translationData.frameCount > 0)
						{
							translation = SampleAnimation(translationData, animation.time, &XMVectorLerp);
						}
						if (rotationData.frameCount > 0)
						{
							rotation = SampleAnimation(rotationData, animation.time, &XMQuaternionSlerp);
						}
						if (scaleData.frameCount > 0)
						{
							scale = SampleAnimation(scaleData, animation.time, &XMVectorLerp);
						}

						node.currentLocal = XMMatrixAffineTransformation(scale, V3_ZERO, rotation, translation);
					}
				}
			}
		}

		// Update joint transforms
		for (int jointIdx = 0; jointIdx < transformHierachy->nodeCount; jointIdx++)
		{
			transformHierachy->UpdateNode(&transformHierachy->nodes[jointIdx]);
		}

		// Upload new transforms to children
		for (int childIdx = 0; childIdx < childCount; childIdx++)
		{
			Entity& child = *children[childIdx];
			if (child.isSkinnedMesh && child.isRendered)
			{
				EntityData& data = child.GetData();
				for (int i = 0; i < transformHierachy->nodeCount; i++)
				{
					assert(i < MAX_BONES);
					data.boneConstantBuffer.data.inverseJointBinds[i] = XMMatrixTranspose(transformHierachy->nodes[i].inverseBind);
					data.boneConstantBuffer.data.jointTransforms[i] = XMMatrixTranspose(transformHierachy->nodes[i].global);
				}
			}
		}
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

void Entity::SetActive(bool newState)
{
	isActive = newState;
	if (isRendered) GetData().visible = newState;
	if (collisionData != nullptr) collisionData->isActive = newState;
}

bool Entity::IsActive()
{
	return isActive;
}

void CalculateDirectionVectors(XMVECTOR& outForward, XMVECTOR& outRight, XMVECTOR& outUp, XMVECTOR inRotation)
{
	XMMATRIX rotationMatrix = XMMatrixRotationQuaternion(inRotation);
	outForward = XMVector3Transform(V3_FORWARD, rotationMatrix);
	outRight = XMVector3Transform(V3_RIGHT, rotationMatrix);
	outUp = XMVector3Cross(outForward, outRight);
}

XMVECTOR SampleAnimation(AnimationData& animData, float animationTime, XMVECTOR(__vectorcall* interp)(XMVECTOR a, XMVECTOR b, float t))
{
	assert(animData.data != nullptr);
	assert(animData.frameCount > 0);
	for (int i = 0; i < animData.frameCount; i++)
	{
		if (animData.times[i] > animationTime)
		{
			if (i == 0)
			{
				return animData.data[0];
			}
			else
			{
				float t = (animationTime - animData.times[i - 1]) / (animData.times[i] - animData.times[i - 1]);
				return interp(animData.data[i - 1], animData.data[i], t);
			}
		}
	}
	return animData.data[animData.frameCount - 1];
}
