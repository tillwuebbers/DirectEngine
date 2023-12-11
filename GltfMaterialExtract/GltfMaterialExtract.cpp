#include <iostream>
#include <string>
#include <filesystem>
#include <format>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_USE_CPP14
#include "import/tiny_gltf.h"

enum TextureFormat : int
{
    BC1 = 15,
    BC5 = 21,
};

void ConvertImage(const std::string& uri, const std::filesystem::path& modelDir, const std::filesystem::path& outputDirectory, TextureFormat format = BC1) {
    std::filesystem::path relativeSourceUri = uri;
    if (relativeSourceUri.is_absolute()) relativeSourceUri = std::filesystem::relative(uri, modelDir);

    std::filesystem::path relativeTargetUri = relativeSourceUri;
    relativeTargetUri.replace_extension(".dds");
    std::cout << "texture " << ("textures" / relativeTargetUri).string() << std::endl;

    std::filesystem::path sourcePath = std::filesystem::absolute(modelDir / relativeSourceUri);
    std::filesystem::path targetPath = outputDirectory / relativeTargetUri;

    std::string command = std::format("nvtt_export.exe -f {} -o {} {}", (int)format, targetPath.string(), sourcePath.string());
    int result = std::system(command.c_str());

    if (result != 0) {
        std::cerr << "Error converting image: " << sourcePath << std::endl;
    }
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input_glb_file> <dds_output_dir>" << std::endl;
        return -1;
    }

    const std::string inputFilename = argv[1];
    const std::string outputDirectory = argv[2];

    tinygltf::Model model;
    tinygltf::TinyGLTF loader;

    std::string err, warn;

    bool result = false;

    if (inputFilename.substr(inputFilename.size() - 4) == "glb") {
		result = loader.LoadBinaryFromFile(&model, &err, &warn, inputFilename.c_str());
    }
    else {
		result = loader.LoadASCIIFromFile(&model, &err, &warn, inputFilename.c_str());
	}

    if (!warn.empty()) {
        std::cerr << "Warning: " << warn << std::endl;
    }

    if (!err.empty()) {
        std::cerr << "Error: " << err << std::endl;
        return -1;
    }

    if (!result) {
        std::cerr << "Failed to parse GLB file." << std::endl;
        return -1;
    }

    std::filesystem::path inputPath = inputFilename;
    std::filesystem::path modelDirectory = inputPath.parent_path();

    int idx = 0;
    for (tinygltf::Material & material : model.materials)
    {
        std::string materialName = material.name;
        if (materialName.empty()) materialName = std::format("{}-{}", inputPath.filename().replace_extension("").string(), idx);
        idx++;

        std::cout << "material " << materialName << " entity" << std::endl;

        int baseColorIndex = material.pbrMetallicRoughness.baseColorTexture.index;
        if (baseColorIndex >= 0)
        {
            tinygltf::Texture& texture = model.textures[baseColorIndex];
            tinygltf::Image& image = model.images[texture.source];
            ConvertImage(image.uri, modelDirectory, outputDirectory);
		}

		int normalIndex = material.normalTexture.index;
        if (normalIndex >= 0)
        {
			tinygltf::Texture& texture = model.textures[normalIndex];
			tinygltf::Image& image = model.images[texture.source];
			ConvertImage(image.uri, modelDirectory, outputDirectory, BC5);
		}

		int metallicRoughnessIndex = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
        if (metallicRoughnessIndex >= 0)
        {
			tinygltf::Texture& texture = model.textures[metallicRoughnessIndex];
			tinygltf::Image& image = model.images[texture.source];
			ConvertImage(image.uri, modelDirectory, outputDirectory);
		}
    }

    return 0;
}