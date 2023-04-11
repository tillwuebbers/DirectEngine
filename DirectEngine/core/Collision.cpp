#include "Collision.h"

void SetCollisionRecord(CollisionRecord& collision, const reactphysics3d::RaycastInfo& info, float rayLength)
{
	collision.worldPoint = XMVectorFromPhysics(info.worldPoint);
	collision.worldNormal = XMVectorFromPhysics(info.worldNormal);
	collision.hitFraction = info.hitFraction;
	collision.distance = rayLength * info.hitFraction;
	collision.triangleIndex = info.triangleIndex;
	collision.body = info.body;
	collision.collider = info.collider;
}