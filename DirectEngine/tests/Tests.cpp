#include "TestCommon.h"

int main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

void AssertVectorEqual(XMVECTOR a, XMVECTOR b, const char* errorPrefix)
{
	EXPECT_NEAR(XMVectorGetX(a), XMVectorGetX(b), TEST_FLOAT_ACCURACY) << errorPrefix << "Vector X different";
	EXPECT_NEAR(XMVectorGetY(a), XMVectorGetY(b), TEST_FLOAT_ACCURACY) << errorPrefix << "Vector Y different";
	EXPECT_NEAR(XMVectorGetZ(a), XMVectorGetZ(b), TEST_FLOAT_ACCURACY) << errorPrefix << "Vector Z different";
	EXPECT_NEAR(XMVectorGetW(a), XMVectorGetW(b), TEST_FLOAT_ACCURACY) << errorPrefix << "Vector W different";
}

void AssertMatrixEqual(XMMATRIX a, XMMATRIX b)
{
	AssertVectorEqual(a.r[0], b.r[0], "Matrix row 0: ");
	AssertVectorEqual(a.r[1], b.r[1], "Matrix row 1: ");
	AssertVectorEqual(a.r[2], b.r[2], "Matrix row 2: ");
	AssertVectorEqual(a.r[3], b.r[3], "Matrix row 3: ");
}