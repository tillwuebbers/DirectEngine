#include "Entity.h"

void Entity::AddChild(Entity* child, bool keepWorldPosition)
{
	assert(child != nullptr);
	if (child == nullptr) return;

	assert(child != this);
	if (child == this) return;

	assert(child->parent == nullptr);
	if (child->parent != nullptr) child->parent->RemoveChild(child, false);
	
	children.newElement() = child;

	child->parent = this;
	child->SetActive(this->IsActive(), false);

	if (keepWorldPosition)
	{
		child->SetLocalPosition(XMVector3Transform(child->worldMatrix.translation, worldMatrix.inverse));
	}
}

void Entity::RemoveChild(Entity* child, bool keepWorldPosition)
{
	assert(child != nullptr);
	if (child == nullptr) return;

	assert(child->parent == this);
	if (child->parent == this) child->parent = nullptr;

	if (keepWorldPosition)
	{
		child->SetLocalPosition(child->worldMatrix.translation);
	}

	children.removeAllEqual(child);
}

void Entity::InitRigidBody(reactphysics3d::PhysicsWorld* physicsWorld, reactphysics3d::BodyType type)
{
	rigidBody = physicsWorld->createRigidBody(GetPhysicsTransform());
	rigidBody->setType(type);
	rigidBody->setUserData(this);
}

void Entity::InitCollisionBody(reactphysics3d::PhysicsWorld* physicsWorld)
{
	collisionBody = physicsWorld->createCollisionBody(GetPhysicsTransform());
	collisionBody->setUserData(this);
}

reactphysics3d::Collider* Entity::InitBoxCollider(reactphysics3d::PhysicsCommon& physicsCommon, XMVECTOR boxExtents, XMVECTOR boxOffset, CollisionLayers collisionLayers, float bounciness, float friction, float density)
{
	assert(rigidBody != nullptr || collisionBody != nullptr);
	assert(rigidBody == nullptr || collisionBody == nullptr);

	reactphysics3d::BoxShape* boxShape = physicsCommon.createBoxShape(PhysicsVectorFromXM(boxExtents));
	reactphysics3d::Transform boxTransform = PhysicsTransformFromXM(boxOffset, XMQuaternionIdentity());

	reactphysics3d::Collider* collider = nullptr;

	if (rigidBody != nullptr)
	{
		collider = rigidBody->addCollider(boxShape, boxTransform);
	}
	if (collisionBody != nullptr)
	{
		collider = collisionBody->addCollider(boxShape, boxTransform);
	}

	if (collider != nullptr)
	{
		collider->setCollisionCategoryBits(static_cast<uint32_t>(collisionLayers));
		collider->setUserData(this);

		reactphysics3d::Material& mat = collider->getMaterial();
		mat.setBounciness(bounciness);
		mat.setFrictionCoefficient(friction);
		mat.setMassDensity(density);
	}

	if (rigidBody != nullptr)
	{
		rigidBody->updateMassPropertiesFromColliders();
	}

	return collider;
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
	if (parent == nullptr)
	{
		worldMatrix.SetMatrix(localMatrix.matrix);
	}
	else
	{
		worldMatrix.SetMatrix(localMatrix.matrix * parent->worldMatrix.matrix);
	}

	if (isRendered)
	{
		GetBuffer().worldTransform = worldMatrix.matrixT;
	}

	for (Entity* child : children)
	{
		child->UpdateWorldMatrix();
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

XMVECTOR Entity::GetForwardDirection()
{
	return XMVector3TransformNormal(V3_FORWARD, worldMatrix.matrix);
}

void Entity::UpdateAudio(EngineCore& engine, const X3DAUDIO_LISTENER* audioListener)
{
	XMVECTOR entityForwards, entityRight, entityUp;
	CalculateDirectionVectors(entityForwards, entityRight, entityUp, worldMatrix.rotation);

	IXAudio2SourceVoice* audioSourceVoice = audioSource.source;
	if (audioSourceVoice != nullptr)
	{
		X3DAUDIO_EMITTER& emitter = audioSource.audioEmitter;
		XMStoreFloat3(&emitter.OrientFront, entityForwards);
		XMStoreFloat3(&emitter.OrientTop, entityUp);
		XMStoreFloat3(&emitter.Position, worldMatrix.translation);

		if (rigidBody != nullptr)
		{
			XMStoreFloat3(&emitter.Velocity, XMVectorFromPhysics(rigidBody->getLinearVelocity()));
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
		for (Entity* child : children)
		{
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

void Entity::UpdatePhysics()
{
	if (collisionBody != nullptr)
	{
		collisionBody->setTransform(GetPhysicsTransform());
	}
	if (rigidBody != nullptr && rigidBody->getType() == reactphysics3d::BodyType::STATIC)
	{
		rigidBody->setTransform(GetPhysicsTransform());
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
	if (rigidBody != nullptr) rigidBody->setIsActive(newState);
	if (collisionBody != nullptr) collisionBody->setIsActive(newState);

	for (Entity* child : children)
	{
		child->SetActive(newState, false);
	}
}

bool Entity::IsActive()
{
	return isParentActive && isSelfActive;
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

XMVECTOR Entity::GetLocalPosition()
{
	return localMatrix.translation;
}

XMVECTOR Entity::GetLocalRotation()
{
	return localMatrix.rotation;
}

XMVECTOR Entity::GetLocalScale()
{
	return localMatrix.scale;
}

void Entity::SetWorldPosition(XMVECTOR worldPos)
{
	if (parent == nullptr)
	{
		SetLocalPosition(worldPos);
	}
	else
	{
		SetLocalPosition(XMVector3Transform(worldPos, parent->worldMatrix.inverse));
	}
}

void Entity::SetWorldRotation(XMVECTOR worldRot)
{
	if (parent == nullptr)
	{
		SetLocalRotation(worldRot);
	}
	else
	{
		SetLocalRotation(XMQuaternionMultiply(XMQuaternionInverse(parent->worldMatrix.rotation), worldRot));
	}
}

void Entity::SetWorldScale(XMVECTOR worldScale)
{
	if (parent == nullptr)
	{
		SetLocalScale(worldScale);
	}
	else
	{
		SetLocalScale(XMVectorDivide(worldScale, parent->worldMatrix.scale));
	}
}

XMVECTOR Entity::GetWorldPosition()
{
	return worldMatrix.translation;
}

XMVECTOR Entity::GetWorldRotation()
{
	return worldMatrix.rotation;
}

XMVECTOR Entity::GetWorldScale()
{
	return worldMatrix.scale;
}

reactphysics3d::Transform Entity::GetPhysicsTransform()
{
	return PhysicsTransformFromXM(worldMatrix.translation, worldMatrix.rotation);
}
