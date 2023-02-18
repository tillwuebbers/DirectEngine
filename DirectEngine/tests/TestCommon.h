#pragma once

#include "gtest/gtest.h"
#include <assert.h>

#include <DirectXMath.h>
using namespace DirectX;

void AssertVectorEqual(XMVECTOR a, XMVECTOR b)
{
	ASSERT_FLOAT_EQ(XMVectorGetX(a), XMVectorGetX(b));
	ASSERT_FLOAT_EQ(XMVectorGetY(a), XMVectorGetY(b));
	ASSERT_FLOAT_EQ(XMVectorGetZ(a), XMVectorGetZ(b));
	ASSERT_FLOAT_EQ(XMVectorGetW(a), XMVectorGetW(b));
}