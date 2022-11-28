#pragma once

#include "Constants.h"

#include <Unknwn.h>
#include <assert.h>

class ComStack
{
public:
	IUnknown** comPointers[MAX_COM_POINTERS];
    size_t pointerIndex = 0;

	void** AddPointer(void** pointer, bool replace = false);
    void Clear();
};

#define NewComObject(stack, pointer) __uuidof(**(pointer)), (stack).AddPointer(IID_PPV_ARGS_Helper(pointer))
#define NewComObjectReplace(stack, pointer) __uuidof(**(pointer)), (stack).AddPointer(IID_PPV_ARGS_Helper(pointer), true)