#define MAX_FRAME_QUEUE 3
#define MAX_DESCRIPTORS 1024
#define MAX_COM_POINTERS 1024
#define MAX_TEXTURES 128
#define MAX_MATERIALS 128
#define MAX_CAMERAS 8
#define MAX_ENTITIES_PER_SCENE 1024
#define MAX_ENTITIES_PER_MATERIAL 128
#define MAX_TEXTURES_PER_MATERIAL 32
#define MAX_ROOT_CONSTANTS_PER_MATERIAL 32
#define MAX_DEFINES_PER_MATERIAL 32
#define MAX_DEBUG_LINE_VERTICES 1024
#define MAX_VERTICES 65536
#define MAX_MESHES 1024
#define MAX_BONES 128
#define MAX_ANIMATIONS 128
#define MAX_CHILDREN 32
#define MAX_ENTITY_CHILDREN 32
#define MAX_COLLISION_RESULTS 128
#define MAX_AUDIO_FILES 64
#define MAX_DEBUG_TEXTURES 32
#define MAX_COLLISION_EVENTS 1024
#define MAX_CONTACT_POINTS 128

#define MAX_PHYSICS_STEP (1. / 60.)

#define GPU_PREFERENCE DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
#define DISPLAY_FORMAT DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
#define DEPTH_BUFFER_FORMAT DXGI_FORMAT_D24_UNORM_S8_UINT
#define DEPTH_BUFFER_FORMAT_TYPELESS DXGI_FORMAT_R24G8_TYPELESS

#define INDEX_BUFFER_FORMAT DXGI_FORMAT_R32_UINT
#define INDEX_BUFFER_TYPE uint32_t

#define SHADOWMAP_SIZE 4096

#define V3_ZERO    XMVECTOR{ 0.f,  0.f, 0.f }
#define V3_ONE     XMVECTOR{ 1.f,  1.f, 1.f }
#define V3_UP      XMVECTOR{ 0.f,  1.f, 0.f }
#define V3_DOWN    XMVECTOR{ 0.f, -1.f, 0.f }
#define V3_RIGHT   XMVECTOR{ 1.f,  0.f, 0.f }
#define V3_FORWARD XMVECTOR{ 0.f,  0.f, 1.f }


#define V4_MASK_X XMVectorSelectControl(XM_SELECT_1, XM_SELECT_0, XM_SELECT_0, XM_SELECT_0)
#define V4_MASK_Y XMVectorSelectControl(XM_SELECT_0, XM_SELECT_1, XM_SELECT_0, XM_SELECT_0)
#define V4_MASK_Z XMVectorSelectControl(XM_SELECT_0, XM_SELECT_0, XM_SELECT_1, XM_SELECT_0)
#define V4_MASK_W XMVectorSelectControl(XM_SELECT_0, XM_SELECT_0, XM_SELECT_0, XM_SELECT_1)

#define V4_MASK_XY XMVectorSelectControl(XM_SELECT_1, XM_SELECT_1, XM_SELECT_0, XM_SELECT_0)
#define V4_MASK_XZ XMVectorSelectControl(XM_SELECT_1, XM_SELECT_0, XM_SELECT_1, XM_SELECT_0)
#define V4_MASK_XW XMVectorSelectControl(XM_SELECT_1, XM_SELECT_0, XM_SELECT_0, XM_SELECT_1)

#define V4_MASK_YZ XMVectorSelectControl(XM_SELECT_0, XM_SELECT_1, XM_SELECT_1, XM_SELECT_0)
#define V4_MASK_YW XMVectorSelectControl(XM_SELECT_0, XM_SELECT_1, XM_SELECT_0, XM_SELECT_1)
#define V4_MASK_ZW XMVectorSelectControl(XM_SELECT_0, XM_SELECT_0, XM_SELECT_1, XM_SELECT_1)

#define V4_MASK_XYZ XMVectorSelectControl(XM_SELECT_1, XM_SELECT_1, XM_SELECT_1, XM_SELECT_0)
#define V4_MASK_XYW XMVectorSelectControl(XM_SELECT_1, XM_SELECT_1, XM_SELECT_0, XM_SELECT_1)
#define V4_MASK_XZW XMVectorSelectControl(XM_SELECT_1, XM_SELECT_0, XM_SELECT_1, XM_SELECT_1)
#define V4_MASK_YZW XMVectorSelectControl(XM_SELECT_0, XM_SELECT_1, XM_SELECT_1, XM_SELECT_1)

#define V4_MASK_NONE XMVectorSelectControl(XM_SELECT_0, XM_SELECT_0, XM_SELECT_0, XM_SELECT_0)
#define V4_MASK_XYZW XMVectorSelectControl(XM_SELECT_1, XM_SELECT_1, XM_SELECT_1, XM_SELECT_1)

#define NDC_MIN_XY -1.
#define NDC_MAX_XY 1.
#define NDC_MIN_Z 0.
#define NDC_MAX_Z 1.

#define SHADER_ENTRY_VS "VSMain"
#define SHADER_ENTRY_PS "PSMain"
#define SHADER_ENTRY_RT "RTMain"
#define SHADER_VERSION_VS "vs_5_1"
#define SHADER_VERSION_PS "ps_5_1"
#define SHADER_VERSION_RT "lib_6_3"

#define LOG_SIZE 1024

#ifdef _DEBUG
#define ISDEBUG true
#else
#define ISDEBUG false
#endif