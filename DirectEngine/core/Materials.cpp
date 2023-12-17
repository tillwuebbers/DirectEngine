#include "Materials.h"
#include <fstream>
#include <sstream>
#include <format>
#include <filesystem>

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
