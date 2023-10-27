#include "Entity.h"

uint64_t g_entityGeneration = 1;

EntityHandle::EntityHandle(Entity* entity)
{
	if (entity == nullptr)
	{
		generation = 0;
	}
	else
	{
		generation = entity->generation;
	}

	ptr = entity;
}

Entity* EntityHandle::Get()
{
	if (ptr == nullptr) return nullptr;
	if (ptr->generation != generation) return nullptr;
	return ptr;
}

bool EntityHandle::operator==(const EntityHandle& other) const
{
	if (ptr == nullptr && other.ptr == nullptr) return true;
	return ptr == other.ptr && generation == other.generation;
}

bool EntityHandle::operator!=(const EntityHandle& other) const
{
	return !(*this == other);
}

// TODO: keepWorldPosition == false apparently doesn't work?
void Entity::AddChild(EntityHandle childHandle, bool keepWorldPosition)
{
	assert(childHandle.Get() != nullptr);
	if (childHandle.Get() == nullptr) return;
	Entity* child = childHandle.Get();

	assert(child != this);
	if (child == this) return;

	if (child->parent.Get() != nullptr) child->parent.Get()->RemoveChild(child, false);
	
	children.newElement() = child;

	child->parent = EntityHandle{ this };
	child->SetActive(this->IsActive(), false);

	if (keepWorldPosition)
	{
		child->SetLocalPosition(XMVector3Transform(child->worldMatrix.translation, worldMatrix.inverse));
	}
}

void Entity::RemoveChild(EntityHandle childHandle, bool keepWorldPosition)
{
	assert(childHandle.Get() != nullptr);
	if (childHandle.Get() == nullptr) return;
	Entity* child = childHandle.Get();

	assert(child->parent.Get() == this);
	if (child->parent.Get() == this) child->parent = EntityHandle{};

	if (keepWorldPosition)
	{
		child->SetLocalPosition(child->worldMatrix.translation);
	}

	children.removeAllEqual(child);
}

void Entity::AddRigidBody(MemoryArena& arena, btDynamicsWorld* world, btCollisionShape* shape, PhysicsInit& physicsInit)
{
	assert(rigidBody == nullptr && world != nullptr);
	if (rigidBody != nullptr || world == nullptr) return;

	assert(physicsInit.type != PhysicsInitType::None);
	if (physicsInit.type == PhysicsInitType::None) return;

	btVector3 inertiaTensor{};
	if (physicsInit.type != PhysicsInitType::RigidBodyDynamic)
	{
		physicsInit.mass = 0.f;
	}
	else
	{
		shape->calculateLocalInertia(physicsInit.mass, inertiaTensor);
	}

	btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(physicsInit.mass, this, shape, inertiaTensor);
	rigidBody = NewObject(arena, btRigidBody, rigidBodyCI);

	if (physicsInit.type == PhysicsInitType::RigidBodyDynamic)
	{
		//assert(rigidBody->getCollisionFlags() & btCollisionObject::CollisionFlags::CF_KINEMATIC_OBJECT);
		//assert(rigidBody->getCollisionFlags() & btCollisionObject::CollisionFlags::CF_STATIC_OBJECT);
	}
	if (physicsInit.type == PhysicsInitType::RigidBodyKinematic) rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() | btCollisionObject::CollisionFlags::CF_KINEMATIC_OBJECT);
	if (physicsInit.type == PhysicsInitType::RigidBodyStatic)    rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() | btCollisionObject::CollisionFlags::CF_STATIC_OBJECT);

	rigidBodyWorld = world;
	world->addRigidBody(rigidBody);
	assert(rigidBody->isInWorld());
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
	if (parent.Get() == nullptr)
	{
		worldMatrix.SetMatrix(localMatrix.matrix);
	}
	else
	{
		worldMatrix.SetMatrix(localMatrix.matrix * parent.Get()->worldMatrix.matrix);
	}

	if (isRendered)
	{
		GetBuffer().worldTransform = worldMatrix.matrixT;
	}

	if (rigidBody != nullptr)
	{
		rigidBody->setWorldTransform(btTransform{ ToBulletQuat(worldMatrix.rotation), ToBulletVec3(worldMatrix.translation + physicsShapeOffset) });
	}

	for (EntityHandle child : children)
	{
		if (child.Get() == nullptr) continue;
		child.Get()->UpdateWorldMatrix();
	}
}

void Entity::SetForwardDirection(XMVECTOR direction, XMVECTOR up, XMVECTOR altUp)
{
	AssertVector3Normalized(direction);
	AssertVector3Normalized(up);
	AssertVector3Normalized(altUp);

	XMVECTOR right;
	if (std::abs(XMVectorGetX(XMVector3Dot(direction, up))) > .99f)
	{
		right = XMVector3Cross(altUp, direction);
	}
	else
	{
		right = XMVector3Cross(up, direction);
	}

	XMVECTOR realUp = XMVector3Cross(direction, right);
	MAT_RMAJ matrix = XMMatrixSet(
		SPLIT_V3(right), 0.f,
		SPLIT_V3(realUp), 0.f,
		SPLIT_V3(direction), 0.f,
		0.f, 0.f, 0.f, 1.f);
	SetWorldRotation(XMQuaternionRotationMatrix(matrix));
}

XMVECTOR Entity::GetForwardDirection() const
{
	return XMVector3TransformNormal(V3_FORWARD, worldMatrix.matrix);
}

void Entity::UpdateAudio(EngineCore& engine, const X3DAUDIO_LISTENER* audioListener)
{
	IXAudio2SourceVoice* audioSourceVoice = audioSource.source;
	if (audioSourceVoice != nullptr)
	{
		X3DAUDIO_EMITTER& emitter = audioSource.audioEmitter;
		XMStoreFloat3(&emitter.OrientFront, worldMatrix.forward);
		XMStoreFloat3(&emitter.OrientTop, worldMatrix.up);
		XMStoreFloat3(&emitter.Position, worldMatrix.translation);

		if (rigidBody != nullptr)
		{
			XMStoreFloat3(&emitter.Velocity, ToXMVec(rigidBody->getLinearVelocity()));
		}
		else
		{
			XMStoreFloat3(&emitter.Velocity, {});
		}

		XAUDIO2_VOICE_DETAILS audioSourceDetails;
		audioSourceVoice->GetVoiceDetails(&audioSourceDetails);

		X3DAUDIO_DSP_SETTINGS* dspSettings = emitter.ChannelCount == 1 ? &engine.m_audioDspSettingsMono : &engine.m_audioDspSettingsStereo;

		X3DAudioCalculate(engine.m_3daudio, audioListener, &audioSource.audioEmitter, X3DAUDIO_CALCULATE_MATRIX | X3DAUDIO_CALCULATE_DOPPLER | X3DAUDIO_CALCULATE_LPF_DIRECT | X3DAUDIO_CALCULATE_REVERB, dspSettings);
		audioSourceVoice->SetOutputMatrix(engine.m_audioMasteringVoice, audioSourceDetails.InputChannels, engine.m_audioVoiceDetails.InputChannels, dspSettings->pMatrixCoefficients);
		audioSourceVoice->SetFrequencyRatio(dspSettings->DopplerFactor);
		// TODO: low pass filter + reverb
	}
}

void Entity::UpdateAnimation(EngineCore& engine, bool isMainRender)
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

			if (animation.active && (isMainRender || !animation.onlyInMainCamera))
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
		for (EntityHandle childHandle : children)
		{
			if (childHandle.Get() == nullptr) continue;
			Entity* child = childHandle.Get();

			if (child->isSkinnedMesh && child->isRendered)
			{
				EntityData& data = child->GetData();
				for (int i = 0; i < transformHierachy->nodeCount; i++)
				{
					assert(i < MAX_BONES);

					BoneMatricesBuffer& boneBuffer = isMainRender ? data.firstPersonBoneConstantBuffer.data : data.boneConstantBuffer.data;
					boneBuffer.inverseJointBinds[i] = XMMatrixTranspose(transformHierachy->nodes[i].inverseBind);
					boneBuffer.jointTransforms[i] = XMMatrixTranspose(transformHierachy->nodes[i].global);
				}
			}
		}
	}
}

void Entity::SetActive(bool newState, bool affectSelf)
{
	if (affectSelf)
	{
		isSelfActive = newState;
	}
	else
	{
		isParentActive = newState;
	}

	if (isRendered) GetData().visible = newState;
	if (rigidBody != nullptr && rigidBodyWorld != nullptr)
	{
		if (!newState && rigidBody->isInWorld()) rigidBodyWorld->removeRigidBody(rigidBody);
		if (newState && !rigidBody->isInWorld()) rigidBodyWorld->addRigidBody(rigidBody);
	}

	for (EntityHandle child : children)
	{
		if (child.Get() == nullptr) continue;
		child.Get()->SetActive(newState, false);
	}
}

bool Entity::IsActive()
{
	return isParentActive && isSelfActive;
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

void Entity::SetLocalPosition(XMVECTOR localPos)
{
	localMatrix.SetMatrix(XMMatrixAffineTransformation(localMatrix.scale, XMVectorZero(), localMatrix.rotation, localPos));
	UpdateWorldMatrix();
}

void Entity::SetLocalRotation(XMVECTOR localRot)
{
	localMatrix.SetMatrix(XMMatrixAffineTransformation(localMatrix.scale, XMVectorZero(), localRot, localMatrix.translation));
	UpdateWorldMatrix();
}

void Entity::SetLocalScale(XMVECTOR localScale)
{
	localMatrix.SetMatrix(XMMatrixAffineTransformation(localScale, XMVectorZero(), localMatrix.rotation, localMatrix.translation));
	UpdateWorldMatrix();
}

XMVECTOR Entity::GetLocalPosition() const
{
	return localMatrix.translation;
}

XMVECTOR Entity::GetLocalRotation() const
{
	return localMatrix.rotation;
}

XMVECTOR Entity::GetLocalScale() const
{
	return localMatrix.scale;
}

void Entity::SetWorldPosition(XMVECTOR worldPos)
{
	if (parent.Get() == nullptr)
	{
		SetLocalPosition(worldPos);
	}
	else
	{
		SetLocalPosition(XMVector3Transform(worldPos, parent.Get()->worldMatrix.inverse));
	}
}

void Entity::SetWorldRotation(XMVECTOR worldRot)
{
	if (parent.Get() == nullptr)
	{
		SetLocalRotation(worldRot);
	}
	else
	{
		SetLocalRotation(XMQuaternionMultiply(XMQuaternionInverse(parent.Get()->worldMatrix.rotation), worldRot));
	}
}

void Entity::SetWorldScale(XMVECTOR worldScale)
{
	if (parent.Get() == nullptr)
	{
		SetLocalScale(worldScale);
	}
	else
	{
		SetLocalScale(XMVectorDivide(worldScale, parent.Get()->worldMatrix.scale));
	}
}

XMVECTOR Entity::GetWorldPosition() const
{
	return worldMatrix.translation;
}

XMVECTOR Entity::GetWorldRotation() const
{
	return worldMatrix.rotation;
}

XMVECTOR Entity::GetWorldScale() const
{
	return worldMatrix.scale;
}

void Entity::getWorldTransform(btTransform& worldTrans) const
{
	worldTrans.setOrigin(ToBulletVec3(worldMatrix.translation + physicsShapeOffset));
	worldTrans.setRotation(ToBulletQuat(worldMatrix.rotation));
}

void Entity::setWorldTransform(const btTransform& worldTrans)
{
	SetWorldPosition(ToXMVec(worldTrans.getOrigin()) - physicsShapeOffset);
	SetWorldRotation(ToXMQuat(worldTrans.getRotation()));
}
