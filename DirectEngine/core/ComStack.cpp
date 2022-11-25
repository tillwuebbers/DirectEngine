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

	OutputDebugString(std::format(L"Adding {}: {}/{}\n", pointerIndex, (void*)*pointer, (void*)pointer).c_str());
	pointerIndex++;
	return pointer;
}

void ComStack::Clear()
{
	while (pointerIndex > 0)
	{
		pointerIndex--;
		IUnknown* unknown = *comPointers[pointerIndex];
		OutputDebugString(std::format(L"Freeing {}: {}/{}\n", pointerIndex, (void*)unknown, (void*)comPointers[pointerIndex]).c_str());
		ULONG pointerCount = unknown->Release();
		OutputDebugString(std::format(L"Count after release: {}\n", pointerCount).c_str());
	}
}