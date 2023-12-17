#include "../DirectEngine/core/Materials.h"

#include <string>
#include <format>
#include <filesystem>
#include <iostream>

int RunDXC(const std::string& inputFilePath, const std::string& includeDir, const std::string& outputPath, const std::string& shaderVersion, const std::string& fileEnding, IArray<ShaderDefine>* defines = nullptr)
{
	std::string definesStr = "";
	if (defines != nullptr)
	{
		for (const ShaderDefine& define : *defines)
		{
			definesStr += std::format("-D{}={} ", define.name.str, define.value.str);
		}
	}

	std::string command = std::format("dxc.exe -T {} -Fo {}.{}.cso -I {} {} \"{}\"", shaderVersion, outputPath, fileEnding, includeDir, definesStr, inputFilePath);
	return std::system(command.c_str());
}

int RunDXC(const std::string& inputFilePath, const std::string& includeDir, const std::string& outputPath, const std::string& shaderVersion, const std::string& fileEnding, const std::string& shaderEntryPoint, IArray<ShaderDefine>* defines = nullptr)
{
	std::string definesStr = "";
	if (defines != nullptr)
	{
		for (const ShaderDefine& define : *defines)
		{
			definesStr += std::format("-D{}={} ", define.name.str, define.value.str);
		}
	}

	std::string command = std::format("dxc.exe -T {} -E \"{}\" -Fo {}.{}.cso -I {} {} \"{}\"", shaderVersion, shaderEntryPoint, outputPath, fileEnding, includeDir, definesStr, inputFilePath);
	return std::system(command.c_str());
}

enum class ShaderCompileGroup : int
{
	Rasterize = 0,
	Raytrace = 1,
	Compute = 2,
};

void CompileShaders(const std::filesystem::path& shadersDir, const std::filesystem::path& shadersIncludeDir, const std::filesystem::path& shadersOutputDir, ShaderCompileGroup compileGroup)
{
	for (const auto& entry : std::filesystem::directory_iterator(shadersDir))
	{
		std::filesystem::path filePath = entry.path();
		std::string filePathStr = filePath.string();
		std::string fileExtension = filePath.extension().string();
		std::string outputPath = (shadersOutputDir / filePath.filename()).string();
		std::string includeDirStr = shadersIncludeDir.string();

		if (fileExtension != ".hlsl") continue;

		int result = 0;
		switch (compileGroup)
		{
		case ShaderCompileGroup::Rasterize:
			result = RunDXC(filePathStr, includeDirStr, outputPath, "vs_6_0", "vert", "VSMain");
			if (result != 0) break;

			result = RunDXC(filePathStr, includeDirStr, outputPath, "ps_6_0", "frag", "PSMain");
			break;

		case ShaderCompileGroup::Raytrace:
			RunDXC(filePathStr, includeDirStr, outputPath, "lib_6_3", "rt");
			break;

		case ShaderCompileGroup::Compute:
			RunDXC(filePathStr, includeDirStr, outputPath, "cs_6_0", "cs", "CSMain");
			break;

		default:
			std::cerr << "Unknown shader compile group: " << (int)compileGroup << std::endl;
			break;
		}

		if (result != 0)
		{
			std::cerr << std::format("Failed to compile shader [error {}]: {}\n", result, filePathStr).c_str() << std::endl;
		}
	}
}

int main(int argc, char** argv)
{
	if (argc != 5)
	{
		std::cerr << "Usage: ShaderPreCompile.exe <shaders dir> <include dir> <output dir> <materials file>" << std::endl;
		return 1;
	}

	std::string shadersDir = argv[1];
	std::string includeDir = argv[2];
	std::string outputDir = argv[3];
	std::string materialsFile = argv[4];

	MemoryArena materialArena = {};
	ArenaArray<MaterialFile> materials = { materialArena, MAX_MATERIALS };
	ArenaArray<TextureFile> textures = { materialArena, MAX_TEXTURES };
	ArenaArray<StandaloneShaderFile> standaloneShaders = { materialArena, 64 };
	LoadMaterials(materialsFile, materials, textures, standaloneShaders);

	for (StandaloneShaderFile& shader : standaloneShaders)
	{
		std::string shaderFileName = std::string{ shader.shaderName.str } + ".hlsl";
		std::string outputPathStr = (std::filesystem::path(outputDir) / shader.shaderName.str).string();

		std::string shaderPathStr = "";

		switch (shader.type)
		{
			case ShaderType::Rasterize:
				shaderPathStr = (std::filesystem::path(shadersDir) / "rasterize" / shaderFileName).string();
				RunDXC(shaderPathStr, includeDir, outputPathStr, "vs_6_0", "vert", "VSMain");
				RunDXC(shaderPathStr, includeDir, outputPathStr, "ps_6_0", "frag", "PSMain");
				break;

			case ShaderType::Compute:
				shaderPathStr = (std::filesystem::path(shadersDir) / "compute" / shaderFileName).string();
				RunDXC(shaderPathStr, includeDir, outputPathStr, "cs_6_0", "cs", "CSMain");
				break;

			case ShaderType::Raytrace:
				shaderPathStr = (std::filesystem::path(shadersDir) / "raytrace" / shaderFileName).string();
				RunDXC(shaderPathStr, includeDir, outputPathStr, "lib_6_3", "rt");
				break;

			default:
				std::cerr << "Unknown shader type: " << (int)shader.type << std::endl;
				break;
		}
	}

	for (MaterialFile& material : materials)
	{
		std::string shaderFileName = std::string{ material.shaderName.str } + ".hlsl";
		std::string shaderPathStr = (std::filesystem::path(shadersDir) / "rasterize" / shaderFileName).string();
		std::string outputPathStr = (std::filesystem::path(outputDir) / material.materialName.str).string();

		RunDXC(shaderPathStr, includeDir, outputPathStr, "vs_6_0", "vert", "VSMain", &material.defines);
		RunDXC(shaderPathStr, includeDir, outputPathStr, "ps_6_0", "frag", "PSMain", &material.defines);
	}
}