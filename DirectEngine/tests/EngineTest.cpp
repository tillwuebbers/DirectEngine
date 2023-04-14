#include "TestCommon.h"

#include "../core/Memory.h"

namespace Engine
{
	struct TestStruct
	{
		int64_t a;
		int64_t b;
	};

	struct SmallStruct
	{
		int64_t a;
	};

	TEST(Memory, Align)
	{
		EXPECT_EQ(Align(0, 8), 0);
		EXPECT_EQ(Align(1, 8), 8);
		EXPECT_EQ(Align(8, 8), 8);
		EXPECT_EQ(Align(9, 8), 16);
		EXPECT_EQ(Align(16, 8), 16);
		EXPECT_EQ(Align(17, 8), 24);
		EXPECT_EQ(Align(24, 8), 24);
		EXPECT_EQ(Align(25, 8), 32);
	}

	TEST(Memory, Allocation)
	{
		MemoryArena arena(1024 * 64 * 2);
		const size_t structSize = sizeof(TestStruct);

		TestStruct* s1 = NewObject(arena, TestStruct);
		s1->a = 1;
		s1->b = 2;
		TestStruct* s2 = NewObject(arena, TestStruct);
		s2->a = 3;
		s2->b = 4;
		EXPECT_EQ(arena.used, structSize * 2);
		EXPECT_EQ(1, *(int64_t*)arena.base);
		EXPECT_EQ(4, *(int64_t*)((uint8_t*)arena.base + sizeof(int64_t) * 3));

		arena.Reset();
		EXPECT_EQ(arena.used, 0);
		TestStruct* s3 = NewObject(arena, TestStruct);
		s3->a = 5;
		s3->b = 6;
		EXPECT_EQ(arena.used, structSize);
		EXPECT_EQ(5, *(int64_t*)arena.base);

		arena.Reset(true);
		EXPECT_EQ(arena.used, 0);

		size_t maxStructs = (1024 * 64 * 2) / structSize;
		TestStruct* testArray = NewArray(arena, TestStruct, maxStructs);
		EXPECT_EQ(arena.used, structSize * maxStructs);
		testArray[0].a = 7;
		testArray[0].b = 8;
		testArray[maxStructs - 1].a = 9;
		testArray[maxStructs - 1].b = 10;
		EXPECT_EQ(7, *(int64_t*)arena.base);
		EXPECT_EQ(9, *(int64_t*)((uint8_t*)arena.base + sizeof(int64_t) * 2 * (maxStructs - 1)));
		EXPECT_EQ(10, *(int64_t*)((uint8_t*)arena.base + sizeof(int64_t) * (2 * (maxStructs - 1) + 1)));
	}

	TEST(Memory, AlignedAllocation)
	{
		MemoryArena arena(1024 * 64 * 2);
		const size_t structSize = sizeof(SmallStruct);

		SmallStruct* s1 = NewObject(arena, SmallStruct);
		s1->a = 1;
		EXPECT_EQ(arena.used, structSize);

		const size_t alignment = 16;
		SmallStruct* s2 = NewObjectAligned(arena, SmallStruct, alignment);

		size_t offset = (uint8_t*)s2 - arena.base;
		size_t globalOffset = (uint8_t*)s2 - (uint8_t*)0;

		EXPECT_EQ(offset % alignment, 0);
		EXPECT_EQ(globalOffset % alignment, 0);

		EXPECT_EQ(arena.used, structSize * 3);
	}
}
