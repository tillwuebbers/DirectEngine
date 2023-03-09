#pragma once
#include "Entity.h"
#include "Log.h"
#include "Mesh.h"
#include "Config.h"
#include "../core/IGame.h"
#include "../core/EngineCore.h"

#include "imgui.h"
#include "ImGuiProfilerRenderer.h"

#include <string>
#include <vector>
#include <mutex>

#include <DirectXMath.h>
using namespace DirectX;

#include <reactphysics3d/reactphysics3d.h>

struct DirectionalLight
{
	XMVECTOR position{ 0.f, 0.f, 0.f };
	XMVECTOR rotation{ 0.f, 0.f, 0.f, 1.f };
};

struct ShadowSpaceBounds
{
	XMFLOAT3 min;
	XMFLOAT3 max;
};

struct Gizmo
{
	Entity* root;
	Entity* translateArrows[3];
	Entity* rotateArrows[3];
	Entity* scaleArrows[3];

	D3D12_VERTEX_BUFFER_VIEW translateArrowMesh;
	D3D12_VERTEX_BUFFER_VIEW rotateArrowMesh;
	D3D12_VERTEX_BUFFER_VIEW scaleArrowMesh;
};

class Game : public IGame
{
public:
	// Memory (keep this up here!)
	MemoryArena globalArena{};
	MemoryArena configArena{};
	TypedMemoryArena<Entity> entityArena{};
	MemoryArena modelArena{};

	// Logging
	bool showLog = ISDEBUG;
	bool stopLog = false;
	RingLog debugLog{};

	bool showProfiler = false;
	bool pauseProfiler = false;
	ImGuiUtils::ProfilersWindow profilerWindow{};
	FrameDebugData lastFrames[256] = {};
	size_t lastDebugFrameIndex = 0;

	// UI
	bool showDemoWindow = false;
	bool showEscMenu = false;
	bool showDebugUI = ISDEBUG;
	bool showDebugImage = false;
	bool showPostProcessWindow = ISDEBUG;
	bool showMovementWindow = false;
	bool showAudioWindow = false;
	bool showEntityList = ISDEBUG;
	bool showLightWindow = false;
	bool scrollLog = true;
	bool noclip = false;
	bool renderPhysics = false;
	bool showInactiveEntities = false;
	bool showLightSpaceDebug = false;
	bool showLightPosition = false;
	bool keepDebugMenuVisibleInGame = ISDEBUG;

	int newChildId = 0;
	int newEntityMaterialIndex = 0;
	float newEntityWidth = 1.f;
	float newEntityHeight = 1.f;
	XMVECTOR physicsForceDebug = {};
	XMVECTOR physicsTorqueDebug = {};

	// Updates
	WindowUpdate windowUpdateData{};
	std::mutex windowUdpateDataMutex;
	EngineInput input{ globalArena };

	// Physics
	reactphysics3d::PhysicsCommon physicsCommon;
	reactphysics3d::PhysicsWorld* physicsWorld;
	MinimumRaycastCallback minRaycastCollector{};
	AllRaycastCallback allRaycastCollector{globalArena};

	// Materials and Lighting
	DirectionalLight light{};
	Texture* diffuseTexture{};
	Texture* memeTexture{};
	XMVECTOR baseClearColor = { .1f, .2f, .4f, 1.f };
	XMVECTOR clearColor = { .1f, .2f, .4f, 1.f };
	XMVECTOR fogColor = { .03f, .01f, .01f, 1.f };

	float contrast = 1.;
	float brightness = 0.;
	float saturation = 1.;
	float fog = 0.;

	// Gizmo
	Gizmo* gizmo = nullptr;
	bool gizmoLocal = true;
	bool editMode = false;
	Entity* editElement = nullptr;
	Entity* selectedGizmoElement = nullptr;
	Entity* selectedGizmoTarget = nullptr;
	XMVECTOR gizmoDragCursorStart{};
	XMVECTOR gizmoDragEntityStart{};

	// Permanent Entities
	Entity* portal1 = nullptr;
	Entity* portal2 = nullptr;
	Entity* playerEntity = nullptr;
	Entity* playerLookEntity = nullptr;
	Entity* cameraEntity = nullptr;
	Entity* playerModelEntity = nullptr;

	// Audio
	AudioSource playerAudioSource{};
	X3DAUDIO_EMITTER playerAudioEmitter{};
	X3DAUDIO_LISTENER playerAudioListener{};
	AudioBuffer* soundFiles[8];

	// Movement
	MovementSettings* movementSettings = nullptr;

	float playerPitch = 0.f;
	float playerYaw = 0.f;
	bool playerOnGround = false;
	
	float inputDeadzone = 0.05f;

	float jumpBufferDuration = 1.f;
	float lastJumpPressTime = -1000.f;

	void StartGame(EngineCore& engine) override;
	void UpdateGame(EngineCore& engine) override;
	void BeforeMainRender(EngineCore& engine) override;
	void DrawUI(EngineCore& engine);
	void DrawDebugUI(EngineCore& engine);

	Entity* CreateEmptyEntity(EngineCore& engine);
	Entity* CreateMeshEntity(EngineCore& engine, size_t drawCallIndex, D3D12_VERTEX_BUFFER_VIEW& meshView);
	Entity* CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height, bool vertical = false, CollisionInitType collisionInit = CollisionInitType::None, CollisionLayers collisionLayers = CollisionLayers::All);
	Entity* CreateEntityFromGltf(EngineCore& engine, const char* path, const std::wstring& shaderName, RingLog& log);
	void UpdateCursorState();

	void PlaySound(EngineCore& engine, AudioSource* audioSource, AudioFile file);
	XMVECTOR ScreenToWorldPosition(EngineCore& engine, CameraData& cameraData, XMVECTOR screenPos);
	void RaycastScreenPosition(EngineCore& engine, CameraData& cameraData, XMVECTOR screenPos, EngineRaycastCallback* callback, CollisionLayers layers = CollisionLayers::All);

	float* GetClearColor() override;
	EngineInput& GetInput() override;

	bool LoadGameConfig();
	void ResetGameConfig();

	void Log(const std::string& message) override;
	void Warn(const std::string& message) override;
	void Error(const std::string& message) override;

	std::mutex& GetWindowUdpateDataMutex() override { return windowUdpateDataMutex; };
	WindowUpdate& GetWindowUpdateData() override { return windowUpdateData; };

	// delete copy and move constructors, we're working with memory arenas here
	Game(const Game&) = delete;
	Game(Game&&) = delete;
	Game& operator=(const Game&) = delete;
	Game& operator=(Game&&) = delete;
};

Gizmo* LoadGizmo(EngineCore& engine, Game& game, size_t materialIndex);

MAT_RMAJ CalculateShadowCamProjection(const MAT_RMAJ& camViewMatrix, const MAT_RMAJ& camProjectionMatrix, const MAT_RMAJ& lightViewMatrix);

void LoadUIStyle();

__declspec(dllexport) IGame* __cdecl CreateGame(MemoryArena& arena);
