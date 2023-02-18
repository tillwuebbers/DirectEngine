#include "TestCommon.h"

#include "../game/Entity.h"
#include "../core/Memory.h"

int main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

TEST(Animation, Sample)
{
	float times[] = { 0.f, 0.25f, 0.75f, 1.f };
	XMVECTOR data[] = { XMVectorSet(0.f, 0.f, 0.f, 0.f), XMVectorSet(1.f, 1.f, 1.f, 1.f), XMVectorSet(2.f, 2.f, 2.f, 2.f), XMVectorSet(3.f, 3.f, 3.f, 3.f) };
	AnimationData animData{};
	animData.frameCount = _countof(times);
	animData.times = times;
	animData.data = data;

	AssertVectorEqual(SampleAnimation(animData, -1.f, XMVectorLerp), { 0.f, 0.f, 0.f, 0.f });
	AssertVectorEqual(SampleAnimation(animData, 0.f, XMVectorLerp), { 0.f, 0.f, 0.f, 0.f });
	AssertVectorEqual(SampleAnimation(animData, 0.25f, XMVectorLerp), { 1.f, 1.f, 1.f, 1.f });
	AssertVectorEqual(SampleAnimation(animData, 0.5f, XMVectorLerp), { 1.5f, 1.5f, 1.5f, 1.5f });
	AssertVectorEqual(SampleAnimation(animData, 0.75f, XMVectorLerp), { 2.f, 2.f, 2.f, 2.f });
	AssertVectorEqual(SampleAnimation(animData, 1.f, XMVectorLerp), { 3.f, 3.f, 3.f, 3.f });
	AssertVectorEqual(SampleAnimation(animData, 1.25f, XMVectorLerp), { 3.f, 3.f, 3.f, 3.f });
}

struct TestStruct
{
	int64_t a;
	int64_t b;
};

TEST(Memory, Align)
{
	ASSERT_EQ(Align(0, 8), 0);
	ASSERT_EQ(Align(1, 8), 8);
	ASSERT_EQ(Align(8, 8), 8);
	ASSERT_EQ(Align(9, 8), 16);
	ASSERT_EQ(Align(16, 8), 16);
	ASSERT_EQ(Align(17, 8), 24);
	ASSERT_EQ(Align(24, 8), 24);
	ASSERT_EQ(Align(25, 8), 32);
}

TEST(Memory, Allocation)
{
	MemoryArena arena(1024 * 64 * 2);
	TestStruct* s1 = NewObject(arena, TestStruct);
	s1->a = 1;
	s1->b = 2;
	TestStruct* s2 = NewObject(arena, TestStruct);
	s2->a = 3;
	s2->b = 4;
	ASSERT_EQ(arena.used, sizeof(TestStruct) * 2);
	ASSERT_EQ(1, *(int64_t*)arena.base);
	ASSERT_EQ(4, *(int64_t*)((uint8_t*)arena.base + sizeof(int64_t) * 3));

	arena.Reset();
	ASSERT_EQ(arena.used, 0);
	TestStruct* s3 = NewObject(arena, TestStruct);
	s3->a = 5;
	s3->b = 6;
	ASSERT_EQ(arena.used, sizeof(TestStruct));
	ASSERT_EQ(5, *(int64_t*)arena.base);

	arena.Reset(true);
	ASSERT_EQ(arena.used, 0);

	size_t maxStructs = (1024 * 64 * 2) / sizeof(TestStruct);
	TestStruct* testArray = NewArray(arena, TestStruct, maxStructs);
	ASSERT_EQ(arena.used, sizeof(TestStruct) * maxStructs);
	testArray[0].a = 7;
	testArray[0].b = 8;
	testArray[maxStructs - 1].a = 9;
	testArray[maxStructs - 1].b = 10;
	ASSERT_EQ(7, *(int64_t*)arena.base);
	ASSERT_EQ(9,  *(int64_t*)((uint8_t*)arena.base + sizeof(int64_t) *  2 * (maxStructs - 1)));
	ASSERT_EQ(10, *(int64_t*)((uint8_t*)arena.base + sizeof(int64_t) * (2 * (maxStructs - 1) + 1)));
}