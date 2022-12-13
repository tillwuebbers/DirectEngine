#define MAX_FRAME_QUEUE 3
#define MAX_DESCRIPTORS 1024
#define MAX_COM_POINTERS 1024
#define MAX_TEXTURE_UPLOADS 32
#define MAX_MATERIALS 32
#define MAX_ENTITIES_PER_MATERIAL 128
#define MAX_TEXTURES_PER_MATERIAL 128
#define MAX_BONES 128
#define MAX_CHILDREN 8
#define DISPLAY_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
#define DEPTH_BUFFER_FORMAT DXGI_FORMAT_D32_FLOAT
#define DEPTH_BUFFER_FORMAT_TYPELESS DXGI_FORMAT_R32_TYPELESS
#define SHADOWMAP_SIZE 1024
#define GPU_PREFERENCE DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE

#define V3_ZERO XMVECTOR{ 0.f, 0.f, 0.f };
#define V3_UP XMVECTOR{ 0.f, 1.f, 0.f }
#define V3_DOWN XMVECTOR{ 0.f, -1.f, 0.f }

#ifdef _DEBUG
#define ISDEBUG true
#else
#define ISDEBUG false
#endif