#include "XR.h"

#include <vector>
#include <format>
#include "../Helpers.h"

inline std::string GetXrVersionString(XrVersion ver)
{
    return std::format("{}.{}.{}", XR_VERSION_MAJOR(ver), XR_VERSION_MINOR(ver), XR_VERSION_PATCH(ver));
}

void InitXR()
{
    // Write out extension properties for a given layer.
    const auto logExtensions = [](const char* layerName, int indent = 0) {
        uint32_t instanceExtensionCount;
        ThrowIfFailed(xrEnumerateInstanceExtensionProperties(layerName, 0, &instanceExtensionCount, nullptr));
        std::vector<XrExtensionProperties> extensions(instanceExtensionCount, { XR_TYPE_EXTENSION_PROPERTIES });
        ThrowIfFailed(xrEnumerateInstanceExtensionProperties(layerName, (uint32_t)extensions.size(), &instanceExtensionCount,
            extensions.data()));

        //OutputDebugString(std::format(L"Available Extensions: ({})\n", instanceExtensionCount).c_str());
        for (const XrExtensionProperties& extension : extensions)
        {
            //OutputDebugString(std::format(L"  SpecVersion {}: {}", extension.extensionVersion, extension.extensionName).c_str());
        }
    };

    // Log non-layer extensions (layerName==nullptr).
    logExtensions(nullptr);

    // Log layers and any of their extensions.
    {
        uint32_t layerCount;
        ThrowIfFailed(xrEnumerateApiLayerProperties(0, &layerCount, nullptr));
        std::vector<XrApiLayerProperties> layers(layerCount, { XR_TYPE_API_LAYER_PROPERTIES });
        ThrowIfFailed(xrEnumerateApiLayerProperties((uint32_t)layers.size(), &layerCount, layers.data()));

        //OutputDebugString(std::format(L"Available Layers: ({})", layerCount).c_str());
        for (const XrApiLayerProperties& layer : layers) {
            //OutputDebugString(std::format(L"  Name={} SpecVersion={} LayerVersion={} Description={}", layer.layerName, GetXrVersionString(layer.specVersion).c_str(), layer.layerVersion, layer.description).c_str());
            logExtensions(layer.layerName, 4);
        }
    }
}