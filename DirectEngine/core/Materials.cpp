#include "Materials.h"
#include <fstream>
#include <sstream>
#include <format>
#include <filesystem>
#include <iostream>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"

TextureFile* CreateTextureFile(MaterialFile* material, ArenaArray<TextureFile>& textures, const std::vector<std::string>& tokens)
{
	if (material == nullptr)
	{
		OutputDebugStringA("Unassociated texture entry!");
		return nullptr;
	}
	if (tokens.size() != 2)
	{
		OutputDebugStringA("Wrong format! Expected texture <path>!");
		return nullptr;
	}

	TextureFile* texture = &textures.newElement();
	texture->texturePath = tokens[1].c_str();
	texture->textureHash = std::hash<std::string>{}(texture->texturePath.str);
	return texture;
}

void LoadMaterials(const std::string& assetListFilePath, ArenaArray<MaterialFile>& materials, ArenaArray<TextureFile>& textures, ArenaArray<StandaloneShaderFile>& standaloneShaders)
{
	std::ifstream file(assetListFilePath);
	assert(file.good());

	std::string line;
	MaterialFile* material = nullptr;

	while (std::getline(file, line))
	{
		if (line.size() == 0) continue;
		if (line[0] == '#') continue;

		std::istringstream iss(line);
		std::vector<std::string> tokens{ std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{} };

		if (tokens.size() == 0) continue;

		// Shaders unassociated with materials
		if (tokens[0] == "shader")
		{
			if (tokens.size() != 3)
			{
				OutputDebugStringA(std::format("Wrong format! Expected shader <name> <shader> <type>: {}\n", line).c_str());
				break;
			}

			StandaloneShaderFile* shader = &standaloneShaders.newElement();
			shader->shaderName = tokens[1].c_str();
			if (tokens[2] == "raster")
			{
				shader->type = ShaderType::Rasterize;
			}
			else if (tokens[2] == "compute")
			{
				shader->type = ShaderType::Compute;
			}
			else if (tokens[2] == "raytrace")
			{
				shader->type = ShaderType::Raytrace;
			}
			else
			{
				OutputDebugStringA(std::format("Invalid shader type: {}\n", line).c_str());
				assert(false);
			}
			continue;
		}

		// New material entry
		if (tokens[0] == "material")
		{
			if (tokens.size() != 3)
			{
				OutputDebugStringA(std::format("Wrong format! Expected material <name> <shader>: {}\n", line).c_str());
				break;
			}

			material = &materials.newElement();
			material->materialName = tokens[1].c_str();
			material->materialHash = std::hash<std::string>{}(material->materialName.str);
			material->shaderName = tokens[2].c_str();
			continue;
		}

		// Below are material properties, need to make sure they connect to a previous material entry
		if (material == nullptr)
		{
			OutputDebugStringA("Unassociated texture entry!");
			assert(false);
			continue;
		}

		if (tokens[0] == "diffuse")
		{
			material->diffuseTexture = CreateTextureFile(material, textures, tokens);
			material->diffuseTexture->isSRGB = true;
			material->defines.newElement() = { "DIFFUSE_TEXTURE", "1" };
		}
		else if (tokens[0] == "diffuse_clip")
		{
			material->diffuseTexture = CreateTextureFile(material, textures, tokens);
			material->defines.newElement() = { "DIFFUSE_TEXTURE", "1" };
			material->defines.newElement() = { "ALPHA_CLIP", "1" };
		}
		else if (tokens[0] == "color")
		{
			if (tokens.size() != 5)
			{
				OutputDebugStringA(std::format("Wrong format! Expected color <r> <g> <b> <a>: {}\n", line).c_str());
				assert(false);
				continue;
			}
			material->diffuseColor = { std::stof(tokens[1]), std::stof(tokens[2]), std::stof(tokens[3]), std::stof(tokens[4]) };
		}
		else if (tokens[0] == "normal")
		{
			material->normalTexture = CreateTextureFile(material, textures, tokens);
			material->defines.newElement() = { "NORMAL_TEXTURE", "1" };
		}
		else if (tokens[0] == "metallic_roughness")
		{
			material->metallicRoughnessTexture = CreateTextureFile(material, textures, tokens);
			material->defines.newElement() = { "METALLIC_ROUGHNESS_TEXTURE", "1" };
		}
		else if (tokens[0] == "cullmode")
		{
			if (tokens.size() != 2)
			{
				OutputDebugStringA(std::format("Wrong format! Expected cullmode <mode>: {}\n", line).c_str());
				assert(false);
				continue;
			}

			if (tokens[1] == "front")
			{
				material->cullMode = D3D12_CULL_MODE_FRONT;
			}
			else if (tokens[1] == "back")
			{
				material->cullMode = D3D12_CULL_MODE_BACK;
			}
			else if (tokens[1] == "none")
			{
				material->cullMode = D3D12_CULL_MODE_NONE;
			}
			else
			{
				OutputDebugStringA(std::format("Invalid cullmode: {}\n", line).c_str());
				assert(false);
				continue;
			}
		}
		else if (tokens[0] == "rootconstant")
		{
			if (tokens.size() != 4)
			{
				OutputDebugStringA(std::format("Wrong format! Expected rootconstant <name> <type> <value>: {}\n", line).c_str());
				assert(false);
				continue;
			}

			if (tokens[2] == "float")
			{
				RootConstantInfo& rootConstant = material->rootConstants.newElement() = { RootConstantType::FLOAT, tokens[1].c_str() };
				rootConstant.defaultValue = std::stof(tokens[3]);
			}
			else if (tokens[2] == "uint")
			{
				RootConstantInfo& rootConstant = material->rootConstants.newElement() = { RootConstantType::UINT, tokens[1].c_str() };
				rootConstant.defaultValue = std::stof(tokens[3]);
			}
			else
			{
				OutputDebugStringA(std::format("Invalid rootconstant type: {}\n", line).c_str());
				assert(false);
				continue;
			}
		}
		else
		{
			OutputDebugStringA(std::format("Invalid materials.txt line: {}\n", line).c_str());
			assert(false);
			continue;
		}
	}
}

HANDLE RunDXC(MemoryArena& arena, const std::string& inputFilePath, const std::string& includeDir, const std::string& outputPath, const std::string& shaderVersion, const std::string& fileEnding, const std::string& shaderEntryPoint = "", IArray<ShaderDefine>*defines = nullptr)
{
	std::string definesStr = "";
	if (defines != nullptr)
	{
		for (const ShaderDefine& define : *defines)
		{
			definesStr += std::format("-D{}={} ", define.name.str, define.value.str);
		}
	}

	std::string command;
	if (shaderEntryPoint.empty())
	{
		command = std::format(" -T {} -Fo {}.{}.cso -I {} {} \"{}\"", shaderVersion, outputPath, fileEnding, includeDir, definesStr, inputFilePath);
	}
	else
	{
		command = std::format(" -T {} -E \"{}\" -Fo {}.{}.cso -I {} {} \"{}\"", shaderVersion, shaderEntryPoint, outputPath, fileEnding, includeDir, definesStr, inputFilePath);
	}
	
	STARTUPINFOA info = { sizeof(info) };
	PROCESS_INFORMATION* processInfo = NewObject(arena, PROCESS_INFORMATION);
	char commandBuffer[2048];
	strcpy_s(commandBuffer, command.c_str());

	if (CreateProcessA("dxc.exe", commandBuffer, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &info, processInfo))
	{
		CloseHandle(processInfo->hThread);
	}
	else
	{
		std::cerr << "Failed to run DXC: " << command << std::endl;
		assert(false);
	}

	return processInfo->hProcess;
}

void CompileShaders(const std::string& shadersDir, const std::string& includeDir, const std::string& outputDir, const std::string& materialsFile)
{
	MemoryArena materialArena = {};
	ArenaArray<MaterialFile> materials = { materialArena, MAX_MATERIALS };
	ArenaArray<TextureFile> textures = { materialArena, MAX_TEXTURES };
	ArenaArray<StandaloneShaderFile> standaloneShaders = { materialArena, 64 };
	LoadMaterials(materialsFile, materials, textures, standaloneShaders);

	ArenaArray<HANDLE> processes = { materialArena, 256 };
	for (StandaloneShaderFile& shader : standaloneShaders)
	{
		std::string shaderFileName = std::string{ shader.shaderName.str } + ".hlsl";
		std::string outputPathStr = (std::filesystem::path(outputDir) / shader.shaderName.str).string();

		std::string shaderPathStr = "";

		switch (shader.type)
		{
		case ShaderType::Rasterize:
			shaderPathStr = (std::filesystem::path(shadersDir) / "rasterize" / shaderFileName).string();
			processes.newElement() = RunDXC(materialArena, shaderPathStr, includeDir, outputPathStr, "vs_6_0", "vert", "VSMain", nullptr);
			processes.newElement() = RunDXC(materialArena, shaderPathStr, includeDir, outputPathStr, "ps_6_0", "frag", "PSMain", nullptr);
			break;

		case ShaderType::Compute:
			shaderPathStr = (std::filesystem::path(shadersDir) / "compute" / shaderFileName).string();
			processes.newElement() = RunDXC(materialArena, shaderPathStr, includeDir, outputPathStr, "cs_6_0", "cs", "CSMain", nullptr);
			break;

		case ShaderType::Raytrace:
			shaderPathStr = (std::filesystem::path(shadersDir) / "raytrace" / shaderFileName).string();
			processes.newElement() = RunDXC(materialArena, shaderPathStr, includeDir, outputPathStr, "lib_6_3", "rt", "", nullptr);
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

		processes.newElement() = RunDXC(materialArena, shaderPathStr, includeDir, outputPathStr, "vs_6_0", "vert", "VSMain", &material.defines);
		processes.newElement() = RunDXC(materialArena, shaderPathStr, includeDir, outputPathStr, "ps_6_0", "frag", "PSMain", &material.defines);
	}

	WaitForMultipleObjectsEx(processes.size, processes.base, TRUE, INFINITE, FALSE);

	for (HANDLE handle : processes)
	{
		CloseHandle(handle);
	}
}