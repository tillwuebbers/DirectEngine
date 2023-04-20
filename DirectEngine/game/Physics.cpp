#include "Physics.h"

void PhysicsDebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
	assert(engine != nullptr);
	if (engine == nullptr) return;

	XMVECTOR xmCol = ToXMVec(color);
	XMVectorSetW(xmCol, 1.f);
	engine->m_debugLineData.AddLine(ToXMVec(from), ToXMVec(to), xmCol, xmCol);
}

void PhysicsDebugDrawer::drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color)
{
	assert(engine != nullptr);
	if (engine == nullptr) return;

	XMVECTOR xmCol = ToXMVec(color);
	XMVectorSetW(xmCol, 1.f);
	engine->m_debugLineData.AddLine(ToXMVec(PointOnB), ToXMVec(PointOnB) + ToXMVec(normalOnB) * distance, xmCol, xmCol);
}

void PhysicsDebugDrawer::reportErrorWarning(const char* warningString)
{
	WARN(warningString);
}

void PhysicsDebugDrawer::draw3dText(const btVector3& location, const char* textString)
{
	// TODO
}

void PhysicsDebugDrawer::setDebugMode(int debugMode)
{
	this->debugMode = debugMode;
}

int PhysicsDebugDrawer::getDebugMode() const
{
	return debugMode;
}
