#define MAX_FRAME_QUEUE 3
#define MAX_DESCRIPTORS 1024
#define MAX_COM_POINTERS 1024
#define MAX_TEXTURE_UPLOADS 32
#define MAX_MATERIALS 32
#define MAX_CAMERAS 8
#define MAX_ENTITIES_PER_SCENE 1024
#define MAX_ENTITIES_PER_MATERIAL 128
#define MAX_TEXTURES_PER_MATERIAL 128
#define MAX_DEBUG_LINE_VERTICES 1024
#define MAX_BONES 128
#define MAX_ANIMATIONS 128
#define MAX_CHILDREN 32
#define MAX_ENTITY_CHILDREN 32
#define MAX_COLLIDERS 1024

#define MAX_PHYSICS_STEP (1. / 30.)

#define GPU_PREFERENCE DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
#define DISPLAY_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
#define DEPTH_BUFFER_FORMAT DXGI_FORMAT_D32_FLOAT
#define DEPTH_BUFFER_FORMAT_TYPELESS DXGI_FORMAT_R32_TYPELESS

#define SHADOWMAP_SIZE 4096

#define IMGUI_DEBUG_TEXTURE_SIZE SHADOWMAP_SIZE
#define IMGUI_DEBUG_TEXTURE_FORMAT DXGI_FORMAT_R32_FLOAT

#define V3_ZERO    XMVECTOR{ 0.f,  0.f, 0.f }
#define V3_ONE     XMVECTOR{ 1.f,  1.f, 1.f }
#define V3_UP      XMVECTOR{ 0.f,  1.f, 0.f }
#define V3_DOWN    XMVECTOR{ 0.f, -1.f, 0.f }
#define V3_RIGHT   XMVECTOR{ 1.f,  0.f, 0.f }
#define V3_FORWARD XMVECTOR{ 0.f,  0.f, 1.f }

#define NDC_MIN_XY -1.
#define NDC_MAX_XY 1.
#define NDC_MIN_Z 0.
#define NDC_MAX_Z 1.

#define SHADER_ENTRY_VS "VSMain"
#define SHADER_ENTRY_PS "PSMain"
#define SHADER_VERSION_VS "vs_5_1"
#define SHADER_VERSION_PS "ps_5_1"

#define DIFFUSE_SUFFIX "_D"

#ifdef GAME_DEBUG
#define ISDEBUG true
#else
#define ISDEBUG false
#endif