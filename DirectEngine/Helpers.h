#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h> // For HRESULT

#undef min
#undef max

#define SPLIT_V3(v) v.m128_f32[0], v.m128_f32[1], v.m128_f32[2]
#define SPLIT_V4(v) v.m128_f32[0], v.m128_f32[1], v.m128_f32[2], v.m128_f32[3]

#define SPLIT_V3_BT(v) v.x(), v.y(), v.z()

#include <chrono>
#include <format>
#include <stdexcept>
#include <DirectXMath.h>
using namespace DirectX;

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

inline XMVECTOR QuaternionToEuler(XMVECTOR quat)
{
    float test = XMVectorGetX(quat) * XMVectorGetY(quat) + XMVectorGetZ(quat) * XMVectorGetW(quat);
    float roll;
    float pitch;
    float yaw;

    if (test > 0.499f) // singularity at north pole
    {
        pitch = 2.f * atan2(XMVectorGetX(quat), XMVectorGetW(quat));
        yaw = XM_PIDIV2;
        roll = 0;
    }
    else if (test < -0.499f) // singularity at south pole
    {
        pitch = -2.f * atan2(XMVectorGetX(quat), XMVectorGetW(quat));
        yaw = -XM_PIDIV2;
        roll = 0.f;
    }
    else
    {
        double sqx = XMVectorGetX(quat) * XMVectorGetX(quat);
        double sqy = XMVectorGetY(quat) * XMVectorGetY(quat);
        double sqz = XMVectorGetZ(quat) * XMVectorGetZ(quat);
        pitch = atan2(2.f * XMVectorGetY(quat) * XMVectorGetW(quat) - 2.f * XMVectorGetX(quat) * XMVectorGetZ(quat), 1.f - 2.f * sqy - 2.f * sqz);
        yaw = asin(2.f * test);
        roll = atan2(2.f * XMVectorGetX(quat) * XMVectorGetW(quat) - 2.f * XMVectorGetY(quat) * XMVectorGetZ(quat), 1.f - 2.f * sqx - 2.f * sqz);
    }
    return { roll, pitch, yaw };
}

inline std::chrono::steady_clock::time_point StartTimer()
{
    return std::chrono::high_resolution_clock::now();
}

inline std::string EndTimer(std::chrono::steady_clock::time_point startTime, std::string funcName, std::string info)
{
    float durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
    std::string message = std::format("{}: {}ms ({})", funcName, durationMs, info);
    std::string messageN = message + "\r\n";
    OutputDebugStringA(messageN.c_str());
    return message;
}

inline std::wstring EndTimer(std::chrono::steady_clock::time_point startTime, std::wstring funcName, std::wstring info)
{
    float durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - startTime).count();
    return std::format(L"{}: {}ms ({})\r\n", funcName, durationMs, info);
}

inline XMVECTOR PlaneNormalForm(XMVECTOR planeNormal, XMVECTOR pointOnPlane)
{
    return XMVectorSetW(planeNormal, XMVectorGetX(-XMVector3Dot(planeNormal, pointOnPlane)));
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

inline void AssertVector3Normalized(XMVECTOR vector)
{
    assert(std::abs(XMVector3Length(vector).m128_f32[0] - 1.f) < 0.001f);
}
