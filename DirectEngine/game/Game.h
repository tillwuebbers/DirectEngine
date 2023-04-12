#pragma once
#include "Entity.h"
#include "Log.h"
#include "Mesh.h"
#include "Config.h"
#include "Physics.h"
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

enum class Material
{
	Test,
	Ground,
	Laser,
	Portal1,
	Portal2,
	Crosshair,
};

/*struct GameTriggerEvent
{
	reactphysics3d::Collider* triggerCollider = nullptr;
	reactphysics3d::CollisionBody* triggerBody = nullptr;
	Entity* triggerEntity = nullptr;
	reactphysics3d::Collider* otherCollider = nullptr;
	reactphysics3d::CollisionBody* otherBody = nullptr;
	Entity* otherEntity = nullptr;

	reactphysics3d::OverlapCallback::OverlapPair::EventType type;
};*/

struct GameContactPoint
{
	XMVECTOR worldNormal = {};
	float penetrationDepth = 0.f;
	XMVECTOR localPointOnShapeA = {};
	XMVECTOR localPointOnShapeB = {};
};

/*struct GameCollisionEvent
{
	reactphysics3d::Collider* colliderA = nullptr;
	reactphysics3d::CollisionBody* bodyA = nullptr;
	Entity* entityA = nullptr;
	reactphysics3d::Collider* colliderB = nullptr;
	reactphysics3d::CollisionBody* bodyB = nullptr;
	Entity* entityB = nullptr;

	CountingArray<GameContactPoint, MAX_CONTACT_POINTS> contactPoints = {};
	reactphysics3d::CollisionCallback::ContactPair::EventType type;
};*/

/*class GamePhysicsEvents : public reactphysics3d::EventListener
{
public:
	virtual void onTrigger(const reactphysics3d::OverlapCallback::CallbackData& callbackData) override;
	virtual void onContact(const reactphysics3d::CollisionCallback::CallbackData& callbackData) override;

	CountingArray<GameTriggerEvent, MAX_COLLISION_EVENTS> triggerEvents{};
	CountingArray<GameCollisionEvent, MAX_COLLISION_EVENTS> collisionEvents{};
};*/

class Game : public IGame
{
public:
	Game(GAME_CREATION_PARAMS);

	// Memory (keep this up here!)
	MemoryArena& globalArena;
	MemoryArena& configArena;
	MemoryArena& levelArena;
	TypedMemoryArena<Entity> entityArena{};

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
	XMVECTOR physicsAddColliderDebug = {};

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

	//reactphysics3d::PhysicsWorld* physicsWorld = nullptr;
	//MinimumRaycastCallback minRaycastCollector{};
	//AllRaycastCallback allRaycastCollector{ globalArena };
	//GamePhysicsEvents physicsEvents{};

	// Materials and Lighting
	std::unordered_map<Material, size_t> materialIndices{};
	
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

	// Entities
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
	MovementSettings* movementSettings = nullptr;
	XMVECTOR defaultPlayerLookPosition = { 0.f, 1.85f, 0.f };

	float playerPitch = 0.f;
	float playerYaw = 0.f;
	bool playerOnGround = false;
	
	float inputDeadzone = 0.05f;

	float jumpBufferDuration = 1.f;
	float lastJumpPressTime = -1000.f;

	void StartGame(EngineCore& engine) override;
	void LoadLevel(EngineCore& engine);
	void UpdateGame(EngineCore& engine) override;
	void BeforeMainRender(EngineCore& engine) override;
	void DrawUI(EngineCore& engine);
	void DrawDebugUI(EngineCore& engine);

	Entity* CreateEmptyEntity(EngineCore& engine);
	Entity* CreateMeshEntity(EngineCore& engine, size_t drawCallIndex, D3D12_VERTEX_BUFFER_VIEW& meshView);
	Entity* CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height, bool vertical = false);
	Entity* CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height, PhysicsInit& physicsInit, bool vertical = false);
	Entity* CreateEntityFromGltf(EngineCore& engine, const char* path, const std::wstring& shaderName, RingLog& log);
	void UpdateCursorState();

	void PlaySound(EngineCore& engine, AudioSource* audioSource, AudioFile file);
	XMVECTOR ScreenToWorldPosition(EngineCore& engine, CameraData& cameraData, XMVECTOR screenPos);
	//void RaycastScreenPosition(EngineCore& engine, CameraData& cameraData, XMVECTOR screenPos, EngineRaycastCallback* callback, CollisionLayers layers = CollisionLayers::All);

	float* GetClearColor() override;
	EngineInput& GetInput() override;

	bool LoadGameConfig();
	void ResetGameConfig();
	void ToggleNoclip();

	void Log(const std::string& message) override;
	void Warn(const std::string& message) override;
	void Error(const std::string& message) override;

	std::mutex& GetWindowUdpateDataMutex() override { return windowUdpateDataMutex; };
	WindowUpdate& GetWindowUpdateData() override { return windowUpdateData; };
};

Gizmo* LoadGizmo(EngineCore& engine, Game& game, size_t materialIndex);

MAT_RMAJ CalculateShadowCamProjection(const MAT_RMAJ& camViewMatrix, const MAT_RMAJ& camProjectionMatrix, const MAT_RMAJ& lightViewMatrix);

void LoadUIStyle();

__declspec(dllexport) IGame* __cdecl CreateGame(GAME_CREATION_PARAMS);
