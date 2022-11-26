#include "ComStack.h"
#include <format>

void** ComStack::AddPointer(void** pointer)
{
	assert(pointer != nullptr);
	assert(pointerIndex < MAX_COM_POINTERS);
	for (int i = 0; i < pointerIndex; i++)
	{
		assert((void*)comPointers[i] != (void*)pointer);
	}

	comPointers[pointerIndex] = (IUnknown**)pointer;

	pointerIndex++;
	return pointer;
}

void ComStack::Clear()
{
	while (pointerIndex > 0)
	{
		pointerIndex--;
		IUnknown* unknown = *comPointers[pointerIndex];
		ULONG pointerCount = unknown->Release();
	}
}