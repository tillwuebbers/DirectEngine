#pragma once

#include "Entity.h"
#include "IGame.h"
#include "Puzzle.h"
#include "Log.h"
#include "Mesh.h"
#include "../core/EngineCore.h"

#include "imgui.h"
#include "ImGuiProfilerRenderer.h"

#include <string>
#include <vector>

#include <DirectXMath.h>
using namespace DirectX;

#define MAX_ENENMY_COUNT 16
#define MAX_PROJECTILE_COUNT 64
#define PLAYER_HAND_OFFSET XMVECTOR{ 0.05f, -.2f, .1f }
#define LASER_LENGTH 100.f

struct WindowUpdate
{
	bool updateCursor;
	bool cursorVisible;
	bool cursorClipped;
};

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
	bool showLog = ISDEBUG;
	bool stopLog = false;
	RingLog debugLog{};

	bool showProfiler = false;
	bool pauseProfiler = false;
	ImGuiUtils::ProfilersWindow profilerWindow{};
	FrameDebugData lastFrames[256] = {};
	size_t lastDebugFrameIndex = 0;

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
	bool showInactiveEntities = false;
	bool showLightSpaceDebug = false;
	bool showLightPosition = false;
	bool keepDebugMenuVisibleInGame = ISDEBUG;

	WindowUpdate windowUpdateData{};
	std::mutex windowUdpateDataMutex;

	MemoryArena globalArena{};
	TypedMemoryArena<Entity> entityArena{};
	MemoryArena modelArena{};
	FixedList<CollisionResult> raycastResult{ globalArena, 32 };

	DirectionalLight light{};
	EngineInput input{ globalArena };
	Texture* diffuseTexture{};
	Texture* memeTexture{};

	Gizmo* gizmo = nullptr;
	bool showGizmo = true;
	bool editMode = false;
	Entity* editElement = nullptr;
	Entity* selectedGizmoElement = nullptr;
	Entity* selectedGizmoTarget = nullptr;
	XMVECTOR gizmoDragCursorStart{};
	XMVECTOR gizmoDragEntityStart{};

	Entity* renderCamParent = nullptr;
	Entity* playerEntity = nullptr;
	Entity* cameraEntity = nullptr;
	Entity* enemies[MAX_ENENMY_COUNT];
	Entity* projectiles[MAX_PROJECTILE_COUNT];
	Entity* laser = nullptr;
	Entity* lightDebugEntity = nullptr;

	AudioSource playerAudioSource{};
	X3DAUDIO_EMITTER playerAudioEmitter{};
	X3DAUDIO_LISTENER playerAudioListener{};

	float playerPitch = 0.f;
	float playerYaw = 0.f;
	XMVECTOR playerVelocity = { 0.f, 0.f, 0.f };

	float playerAcceleration = 125.f;
	float playerFriction = 125.f;
	float playerMaxSpeed = 20.f;
	float playerJumpStrength = 15.f;
	float playerGravity = 35.f;
	float inputDeadzone = 0.05f;
	bool autojump = true;
	bool spawnEnemies = false;

	float enemyAcceleration = 200.f;
	float enemyMaxSpeed = 15.f;
	float enemySpawnRate = 5.f;

	float projectileSpeed = 100.f;
	float projectileSpawnRate = .1f;
	float projectileLifetime = 1.f;
	float laserSpawnRate = .5f;
	float laserLifetime = .4f;

	float jumpBufferDuration = 1.f;
	float lastJumpPressTime = -1000.f;
	float lastEnemySpawn = 0.f;
	float lastProjectileSpawn = -1000.f;
	float lastLaserSpawn = -1000.f;

	XMVECTOR baseClearColor = { .1f, .2f, .4f, 1.f };
	XMVECTOR clearColor = { .1f, .2f, .4f, 1.f };
	XMVECTOR fogColor = { .03f, .01f, .01f, 1.f };
	
	float contrast = 1.;
	float brightness = 0.;
	float saturation = 1.;
	float fog = 0.;

	int newChildId = 0;
	int newEntityMaterialIndex = 0;
	float newEntityWidth = 1.f;
	float newEntityHeight = 1.f;

	AudioBuffer* soundFiles[8];

	void StartGame(EngineCore& engine) override;
	void UpdateGame(EngineCore& engine) override;
	void DrawUI(EngineCore& engine);
	void DrawDebugUI(EngineCore& engine);

	Entity* CreateEmptyEntity(EngineCore& engine);
	Entity* CreateMeshEntity(EngineCore& engine, size_t drawCallIndex, D3D12_VERTEX_BUFFER_VIEW& meshView);
	Entity* CreateQuadEntity(EngineCore& engine, size_t materialIndex, float width, float height);
	Entity* CreateEntityFromGltf(EngineCore& engine, const char* path, const std::wstring& shaderName, RingLog& log);
	void UpdateCursorState();

	void PlaySound(EngineCore& engine, AudioSource* audioSource, AudioFile file);
	XMVECTOR ScreenToWorldPosition(EngineCore& engine, CameraData& cameraData, XMVECTOR screenPos);
	CollisionResult RaycastScreenPosition(EngineCore& engine, CameraData& cameraData, XMVECTOR screenPos, CollisionLayers layers);
	void RaycastScreenPositionAll(EngineCore& engine, CameraData& cameraData, XMVECTOR screenPos, CollisionLayers layers, FixedList<CollisionResult>& result);

	float* GetClearColor() override;
	EngineInput& GetInput() override;

	void Log(const std::string& message) override;
	void Warn(const std::string& message) override;
	void Error(const std::string& message) override;
};


Gizmo* LoadGizmo(EngineCore& engine, Game& game, size_t materialIndex);

MAT_RMAJ CalculateShadowCamProjection(const MAT_RMAJ& camViewMatrix, const MAT_RMAJ& camProjectionMatrix, const MAT_RMAJ& lightViewMatrix);

void LoadUIStyle();