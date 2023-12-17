#include "../DirectEngine/core/Materials.h"

#include <iostream>

int main(int argc, char** argv)
{
	if (argc != 5)
	{
		std::cerr << "Usage: ShaderPreCompile.exe <shaders dir> <include dir> <output dir> <materials file>" << std::endl;
		return 1;
	}

	CompileShaders(argv[1], argv[2], argv[3], argv[4]);
}