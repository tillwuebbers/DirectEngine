#pragma once
#include "Entity.h"
#include "IEntityCreator.h"
#include "Mesh.h"
#include "Config.h"
#include "Physics.h"
#include "Gizmo.h"
#include "Assets.h"
using namespace Assets;

#include "../core/IGame.h"
#include "../core/EngineCore.h"

#include "imgui.h"
#include "ImGuiProfilerRenderer.h"

#include <string>
#include <vector>
#include <mutex>

#include <DirectXMath.h>
using namespace DirectX;

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

struct GameContactPoint
{
	XMVECTOR worldNormal = {};
	float penetrationDepth = 0.f;
	XMVECTOR localPointOnShapeA = {};
	XMVECTOR localPointOnShapeB = {};
};

struct TimeData
{
	float deltaTime = 0.f;
	float timeSinceStart = 0.f;
};

struct PlayerMovement
{
	MovementSettings* movementSettings = nullptr;

	bool noclip = false;
	bool playerOnGround = false;

	float lastJumpPressTime = -1000.f;
	float inputDeadzone = 0.05f;
	float jumpBufferDuration = 1.f;


	void Update(EngineInput& input, TimeData& time, Entity* playerEntity, Entity* playerLookEntity, Entity* cameraEntity, btDynamicsWorld* dynamicsWorld, bool frameStep);
};

class Game : public IGame, public IEntityCreator
{
public:
	Game(GAME_CREATION_PARAMS);

	// Memory (keep this up here!)
	MemoryArena& globalArena;               // Never cleared
	MemoryArena& configArena;
	MemoryArena& levelArena;                // Cleared on every level reload
	TypedMemoryArena<Entity> entityArena{}; // Cleared on every level reload
	
	// Logging
	bool showLog = ISDEBUG;
	bool stopLog = false;

	bool showProfiler = false;
	bool pauseProfiler = false;
	ImGuiUtils::ProfilersWindow profilerWindow{};
	FrameDebugData lastFrames[256] = {};
	size_t lastDebugFrameIndex = 0;

	// UI
	bool showDemoWindow = false;
	bool showEscMenu = false;
	bool showDebugUI = ISDEBUG;
	bool showPostProcessWindow = ISDEBUG;
	bool showMovementWindow = false;
	bool showAudioWindow = false;
	bool showEntityList = ISDEBUG;
	bool showMaterialList = false;
	bool showLightWindow = false;
	bool scrollLog = true;
	bool noclip = false;
	bool renderPhysics = false;
	bool showInactiveEntities = false;
	bool showLightSpaceDebug = false;
	bool showLightPosition = false;
	bool showMatrixCalculator = false;
	bool keepDebugMenuVisibleInGame = ISDEBUG;

	bool frameStep = false;
	bool levelLoaded = false;

	int newChildId = 0;
	int newEntityMaterialIndex = 0;
	float newEntityWidth = 1.f;
	float newEntityHeight = 1.f;
	XMVECTOR physicsForceDebug = {};
	XMVECTOR physicsTorqueDebug = {};

	CameraData* matrixCalcSelectedCam = nullptr;
	XMVECTOR matrixCalcInputVec = {};
	XMMATRIX matrixCalcModelMatrix = XMMatrixIdentity();
	XMMATRIX matrixCalcViewMatrix = XMMatrixIdentity();
	XMMATRIX matrixCalcProjectionMatrix = XMMatrixIdentity();

	// Updates
	WindowUpdate windowUpdateData{};
	std::mutex windowUdpateDataMutex;
	EngineInput input{ globalArena };

	// Physics
	btDefaultCollisionConfiguration* collisionConfiguration = nullptr;
	btCollisionDispatcher* dispatcher = nullptr;
	btBroadphaseInterface* broadphase = nullptr;
	btSequentialImpulseConstraintSolver* solver = nullptr;
	btDiscreteDynamicsWorld* dynamicsWorld = nullptr;
	PhysicsDebugDrawer physicsDebug{};

	// Meshes
	MeshData* cubeMeshData = nullptr;
	FixedList<MeshData*> level1MeshData = { levelArena, 4 };
	btBvhTriangleMeshShape* levelShape = nullptr;

	// Materials and Lighting
	std::unordered_map<Material, size_t> materialIndices{};
	
	DirectionalLight light{};
	XMVECTOR baseClearColor = { .1f, .2f, .4f, 1.f };
	XMVECTOR clearColor = { .1f, .2f, .4f, 1.f };
	XMVECTOR fogColor = { .03f, .01f, .01f, 1.f };

	float exposure = 1.0f;
	float contrast = 1.0f;
	float brightness = 0.0f;
	float saturation = 1.0f;
	float fog = 0.0f;

	Gizmo gizmo{};

	// Entities
	Entity* skybox = nullptr;
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
	CountingArray<AudioBuffer*, MAX_AUDIO_FILES> soundFiles{};

	// Movement
	PlayerMovement playerMovement{};
	XMVECTOR defaultPlayerLookPosition = { 0.f, 1.85f, 0.f };

	// Player
	float playerPitch = 0.f;
	float playerYaw = 0.f;
	
	void RegisterLog(EngineLog::RingLog* log) override;
	void StartGame(EngineCore& engine) override;
	void LoadLevel(EngineCore& engine);
	void UpdateGame(EngineCore& engine) override;
	void BeforeMainRender(EngineCore& engine) override;
	void DrawUI(EngineCore& engine);
	void DrawDebugUI(EngineCore& engine);

	Entity* CreateEmptyEntity(EngineCore& engine) override;
	Entity* CreateMeshEntity(EngineCore& engine, size_t drawCallIndex, MeshData* meshData) override;
	Entity* CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height, bool vertical = false) override;
	Entity* CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height, PhysicsInit& physicsInit, bool vertical = false) override;
	Entity* CreateEntityFromGltf(EngineCore& engine, const char* path, Assets::Shader shader) override;
	void UpdateCursorState();

	void PlaySound(EngineCore& engine, AudioSource* audioSource, AudioFile file);
	XMVECTOR ScreenToWorldPosition(EngineCore& engine, CameraData& cameraData, XMVECTOR screenPos);
	//void RaycastScreenPosition(EngineCore& engine, CameraData& cameraData, XMVECTOR screenPos, EngineRaycastCallback* callback, CollisionLayers layers = CollisionLayers::All);

	float* GetClearColor() override;
	EngineInput& GetInput() override;

	bool LoadGameConfig();
	void ResetGameConfig();
	void ToggleNoclip();

	std::mutex& GetWindowUdpateDataMutex() override { return windowUdpateDataMutex; };
	WindowUpdate& GetWindowUpdateData() override { return windowUpdateData; };
};

MAT_RMAJ CalculateShadowCamProjection(const MAT_RMAJ& camViewMatrix, const MAT_RMAJ& camProjectionMatrix, const MAT_RMAJ& lightViewMatrix);

void LoadUIStyle();

__declspec(dllexport) IGame* __cdecl CreateGame(GAME_CREATION_PARAMS);
