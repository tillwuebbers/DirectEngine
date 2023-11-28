#pragma once

#include "../core/Memory.h"
#include "../core/Common.h"
#include "../core/Constants.h"

#include <unordered_map>

namespace Assets
{
	enum class Material
	{
		Ground,
		Laser,
		Portal1,
		Portal2,
		Crosshair,
		RTOutput,
		ShellTexture,
	};

	enum class Shader
	{
		Entity,
		EntitySkinned,
		Ground,
		Laser,
		Crosshair,
		TextureQuad,
		Portal,
		ShellTex,
	};

	static const std::unordered_map<Shader, std::wstring> SHADER_NAMES = {
		{ Shader::Entity, L"entity" },
		{ Shader::EntitySkinned, L"entity-skinned" },
		{ Shader::Ground, L"ground" },
		{ Shader::Laser, L"laser" },
		{ Shader::Crosshair, L"crosshair" },
		{ Shader::TextureQuad, L"texturequad" },
		{ Shader::Portal, L"portal" },
		{ Shader::ShellTex, L"shelltexture" },
	};
}