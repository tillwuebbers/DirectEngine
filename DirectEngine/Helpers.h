#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h> // For HRESULT

#undef min
#undef max

#include <chrono>
#include <format>
#include <stdexcept>

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

inline std::chrono::steady_clock::time_point StartTimer()
{
    return std::chrono::high_resolution_clock::now();
}

inline std::string EndTimer(std::chrono::steady_clock::time_point startTime, std::string funcName, std::string info)
{
    float durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
    return std::format("{}: {}ms ({})", funcName, durationMs, info);
}

inline std::wstring EndTimer(std::chrono::steady_clock::time_point startTime, std::wstring funcName, std::wstring info)
{
    float durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
    return std::format(L"{}: {}ms ({})\r\n", funcName, durationMs, info);
}

#define WIDE2(x) L##x
#define WIDE1(x) WIDE2(x)

#ifdef ENABLE_TIMERS
#define INIT_TIMER(t) std::chrono::steady_clock::time_point t = StartTimer()
#define RESET_TIMER(t) t = StartTimer()
#define LOG_TIMER(t, s, l) l.Log(EndTimer(t, __FUNCTION__, s))
#define OUTPUT_TIMERW(t, s) OutputDebugString(EndTimer(t, WIDE1(__FUNCTION__), s).c_str())
#else
#define INIT_TIMER(t) std::chrono::steady_clock::time_point t
#define RESET_TIMER(t)
#define LOG_TIMER(t, s, l)
#define OUTPUT_TIMERW(t, s)
#endif

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
