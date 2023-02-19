#pragma once

#include "gtest/gtest.h"
#include <assert.h>

#include <DirectXMath.h>
using namespace DirectX;

#define TEST_FLOAT_ACCURACY .0001f

void AssertVectorEqual(XMVECTOR a, XMVECTOR b, const char* errorPrefix = "");

void AssertMatrixEqual(XMMATRIX a, XMMATRIX b);