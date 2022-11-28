#include "ComStack.h"
#include <format>

// By default there's something wrong if we write the same com pointer twice, only allow this when explicitly set
void** ComStack::AddPointer(void** pointer, bool replace)
{
	assert(pointer != nullptr);
	for (int i = 0; i < pointerIndex; i++)
	{
		bool match = (void*)comPointers[i] == (void*)pointer;
		assert(!match || replace);
		if (match)
		{
			(*comPointers[i])->Release();
			comPointers[i] = (IUnknown**)pointer;
			return pointer;
		}
	}

	assert(pointerIndex < MAX_COM_POINTERS);
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
		unknown->Release();
	}
}