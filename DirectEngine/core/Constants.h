#define MAX_FRAME_QUEUE 3
#define MAX_DESCRIPTORS 512
#define DISPLAY_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
#define SHADOWMAP_SIZE 1024

#define V3_ZERO XMVECTOR{ 0.f, 0.f, 0.f };
#define V3_UP XMVECTOR{ 0.f, 1.f, 0.f }
#define V3_DOWN XMVECTOR{ 0.f, -1.f, 0.f }

#ifdef _DEBUG
#define ISDEBUG true
#else
#define ISDEBUG false
#endif