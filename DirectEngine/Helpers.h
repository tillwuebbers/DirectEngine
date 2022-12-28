#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h> // For HRESULT

#include <format>
#include <stdexcept>

#ifdef START_WITH_XR
#include "openxr/openxr.h"
#include <openxr/openxr_reflection.h>

// https://github.com/KhronosGroup/OpenXR-SDK-Source/blob/main/src/tests/hello_xr/common.h
// Macro to generate stringify functions for OpenXR enumerations based data provided in openxr_reflection.h
// clang-format off
#define ENUM_CASE_STR(name, val) case name: return #name;
#define MAKE_TO_STRING_FUNC(enumType)                  \
    inline const char* to_string(enumType e) {         \
        switch (e) {                                   \
            XR_LIST_ENUM_##enumType(ENUM_CASE_STR)     \
            default: return "Unknown " #enumType;      \
        }                                              \
    }

MAKE_TO_STRING_FUNC(XrReferenceSpaceType);
MAKE_TO_STRING_FUNC(XrViewConfigurationType);
MAKE_TO_STRING_FUNC(XrEnvironmentBlendMode);
MAKE_TO_STRING_FUNC(XrSessionState);
MAKE_TO_STRING_FUNC(XrResult);
MAKE_TO_STRING_FUNC(XrFormFactor);
#endif

inline std::string HrToString(HRESULT hr)
{
    char s_str[64] = {};
    sprintf_s(s_str, "0x%08X", static_cast<UINT>(hr));
    return std::string(s_str);
}

inline void Throw(std::string errorMessage, const char* originator, const char* sourceLocation)
{
    throw std::logic_error(std::format("Origin: {}\nSource: {}\n{}\n", originator, sourceLocation, errorMessage));
}

#ifdef START_WITH_XR

[[noreturn]] inline void ThrowXrResult(XrResult res, const char* originator = nullptr, const char* sourceLocation = nullptr) {
    Throw(std::format("XrResult failure [{}]", to_string(res)), originator, sourceLocation);
}

inline XrResult ThrowIfFailed(XrResult result, const char* originator = nullptr, const char* sourceLocation = nullptr)
{
    if (XR_FAILED(result)) {
        ThrowXrResult(result, originator, sourceLocation);
    }

    return result;
}

#endif

inline HRESULT ThrowIfFailed(HRESULT hr, const char* originator = nullptr, const char* sourceLocation = nullptr)
{
    if (FAILED(hr))
    {
        Throw(std::format("HResult failiure [{}]", HrToString(hr)), originator, sourceLocation);
    }
    return hr;
}

template <uint32_t alignment>
constexpr uint32_t AlignTo(uint32_t n) {
    static_assert((alignment & (alignment - 1)) == 0, "The alignment must be power-of-two");
    return (n + alignment - 1) & ~(alignment - 1);
}

#define CHK_STRINGIFY(x) #x
#define TOSTRING(x) CHK_STRINGIFY(x)
#define FILE_AND_LINE __FILE__ ":" TOSTRING(__LINE__)
#define CHECK_HRCMD(cmd) ThrowIfFailed(cmd, #cmd, FILE_AND_LINE);
#define CHECK_HRESULT(res, cmdStr) ThrowIfFailed(res, cmdStr, FILE_AND_LINE);

class HrException : public std::runtime_error
{
public:
    HrException(HRESULT hr) : std::runtime_error(HrToString(hr)), m_hr(hr) {}
    HRESULT Error() const { return m_hr; }
private:
    const HRESULT m_hr;
};
