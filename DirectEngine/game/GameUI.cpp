#include "Game.h"
#include "remixicon.h"
#include "../core/UI.h"

#include <numeric>

#define SLIDER_SPEED 0.005f
#define SLIDER_MIN (-1000.f)
#define SLIDER_MAX 1000.f

XMVECTOR GetMatrixColumn(XMMATRIX& mat, size_t index)
{
	assert(index >= 0 && index < 4 && "Invalid matrix column index");
	XMVECTOR result{};
	for (int i = 0; i < 4; i++)
	{
		result.m128_f32[i] = mat.r[i].m128_f32[index];
	}
	return result;
}

void SetMatrixColumn(XMMATRIX& mat, size_t index, XMVECTOR& value)
{
	assert(index >= 0 && index < 4 && "Invalid matrix column index");
	for (int i = 0; i < 4; i++)
	{
		mat.r[i].m128_f32[index] = value.m128_f32[i];
	}
}

void DisplayMatrix(XMMATRIX& mat)
{
	XMVECTOR scale, rotation, translation;
	XMMatrixDecompose(&scale, &rotation, &translation, mat);

	ImGui::Text("%.1f %.1f %.1f %.1f", SPLIT_V4(mat.r[0]));
	ImGui::SameLine(0.f, 10.f);
	ImGui::Text("Position: (%.1f, %.1f, %.1f)", SPLIT_V3(translation));

	ImGui::Text("%.1f %.1f %.1f %.1f", SPLIT_V4(mat.r[1]));
	ImGui::SameLine(0.f, 10.f);
	ImGui::Text("Rotation: (%.1f, %.1f, %.1f, %.1f)", SPLIT_V4(rotation));
	
	ImGui::Text("%.1f %.1f %.1f %.1f", SPLIT_V4(mat.r[2]));
	ImGui::SameLine(0.f, 10.f);
	ImGui::Text("Scale: (%.1f, %.1f, %.1f)", SPLIT_V3(scale));
	
	ImGui::Text("%.1f %.1f %.1f %.1f", SPLIT_V4(mat.r[3]));
}

void DisplayTransformNode(TransformNode* node)
{
	if (node->childCount > 0)
	{
		if (ImGui::TreeNodeEx(node->name.c_str()))
		{
			for (int i = 0; i < node->childCount; i++)
			{
				DisplayTransformNode(node->children[i]);
			}
			ImGui::TreePop();
		}
	}
	else
	{
		ImGui::Text(node->name.c_str());
	}
}

void InputSizeT(const char* label, size_t* value)
{
	size_t defaultStep = 1;
	size_t defaultStepBig = 100;
	ImGui::InputScalar(label, ImGuiDataType_U64, value, (void*)&defaultStep, (void*)&defaultStepBig, "%llu", 0);
}

void InputMatrix(XMMATRIX& mat, const char* id)
{
	XMVECTOR scale, rotation, translation;
	XMMatrixDecompose(&scale, &rotation, &translation, mat);

	ImGui::PushID(id);
	ImGui::PushItemWidth(-FLT_MIN);
	for (int i = 0; i < 4; i++)
	{
		XMVECTOR col = GetMatrixColumn(mat, i);
		if (ImGui::InputFloat4(("##c" + std::to_string(i)).c_str(), &col.m128_f32[0]))
		{
			SetMatrixColumn(mat, i, col);
		}
	}
	ImGui::PopItemWidth();
	ImGui::PopID();
}

void Game::DrawUI(EngineCore& engine)
{
	engine.BeginProfile("Game UI", ImColor::HSV(.75f, 1.f, .75f));
	if (showDemoWindow) ImGui::ShowDemoWindow(&showDemoWindow);

	// Escape Menu
	if (showEscMenu && !gizmo.editMode)
	{
		const int buttonWidth = 250;
		const int buttonHeight = 30;
		const ImVec2 buttonSize(buttonWidth, buttonHeight);
		const float hAlign = (engine.m_width - buttonWidth) / 2.f;
		const float vAlign = (engine.m_height - (buttonHeight * 3 + ImGui::GetStyle().ItemSpacing.y * 2)) / 2.f;

		ImGui::SetNextWindowSize(ImVec2(static_cast<float>(engine.m_width), static_cast<float>(engine.m_height)));
		ImGui::SetNextWindowPos(ImVec2());
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(hAlign, vAlign));
		ImGui::Begin("Pause Menu", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground);

		if (ImGui::Button("Continue", buttonSize))
		{
			showEscMenu = false;
			UpdateCursorState();
		}
		if (ImGui::Button("Show Debug Menu", buttonSize))
		{
			showDebugUI = true;
		}
		if (ImGui::Button("Quit", buttonSize))
		{
			engine.m_quit = true;
		}

		ImGui::End();
		ImGui::PopStyleVar(2);
	}

	// Debug UI
	if (showEscMenu || keepDebugMenuVisibleInGame)
	{
		DrawDebugUI(engine);
	}
}

void Game::DrawDebugUI(EngineCore& engine)
{
	if (showDebugUI)
	{
		std::string title = std::format("{} Debug", ICON_BUG_FILL);

		ImGui::SetNextWindowPos(ImVec2{ 0, 0 });
		if (ImGui::Begin(title.c_str(), &showDebugUI, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Checkbox("Keep Visible", &keepDebugMenuVisibleInGame);
			ImGui::SameLine();
			ImGui::Checkbox("Edit Mode", &gizmo.editMode);

			if (ImGui::Button("Reset Config"))
			{
				ResetGameConfig();
			}
			ImGui::SameLine();
			if (ImGui::Button("Save Config"))
			{
				SaveConfig(configArena, CONFIG_PATH);
			}
			ImGui::SameLine();
			if (ImGui::Button("Noclip"))
			{
				ToggleNoclip();
			}

			ImGui::Text("Camera Position: %.1f %.1f %.1f", SPLIT_V3(engine.mainCamera->worldMatrix.translation));
			ImGui::Text("Camera Rotation: %.1f %.1f", playerPitch / XM_2PI * 360.f, playerYaw / XM_2PI * 360.f);
			ImGui::Text("Player on Ground: %s", playerMovement.playerOnGround ? "true" : "false");

			ImGui::NewLine();

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			const float maxDisplayedFrameTime = (1.f / 144.f) * 2.f;
			const float lineWidth = 4.f;
			ImVec2 pos = ImGui::GetCursorPos();
			ImVec2 max = ImVec2(ImGui::GetWindowContentRegionMax().x, pos.y + 80);
			int rectWidth = static_cast<int>(ImGui::GetWindowContentRegionWidth());
			float rectHeight = max.y - pos.y;

			drawList->AddRectFilled(pos, max, ImColor::HSV(0, 0, 0), 3);
			drawList->AddLine(ImVec2(pos.x, pos.y + rectHeight / 2.f), ImVec2(max.x, pos.y + rectHeight / 2.f), ImColor::HSV(0, 0, .5f));

			int drawIndex = static_cast<int>((rectWidth - 1) / lineWidth);
			while (drawIndex >= 0)
			{
				int frameIndex = lastDebugFrameIndex - drawIndex;
				while (frameIndex < 0) frameIndex += _countof(lastFrames);

				float linePercentHeight = lastFrames[frameIndex].duration / maxDisplayedFrameTime;
				float p1Y = pos.y + rectHeight * (1.f - linePercentHeight);
				drawList->AddLine(ImVec2(pos.x + drawIndex * lineWidth, p1Y), ImVec2(pos.x + drawIndex * lineWidth, max.y), ImColor(0.f, 1.f, 0.f), lineWidth);

				drawIndex--;
			}

			if (ImGui::Button(pauseProfiler ? ICON_PLAY_FILL : ICON_PAUSE_FILL))
			{
				pauseProfiler = !pauseProfiler;
			}

			ImGui::SetCursorPos(max);

			ImGui::NewLine();

			if (ImGui::Button("ImGui Demo"))
			{
				showDemoWindow = !showDemoWindow;
			}
			ImGui::SameLine();
			if (ImGui::Button("Log"))
			{
				showLog = !showLog;
			}
			ImGui::SameLine();
			if (ImGui::Button("Profiler"))
			{
				showProfiler = !showProfiler;
			}

			if (ImGui::Button("Movement"))
			{
				showMovementWindow = !showMovementWindow;
			}
			ImGui::SameLine();
			if (ImGui::Button("Graphics"))
			{
				showPostProcessWindow = !showPostProcessWindow;
			}
			ImGui::SameLine();
			if (ImGui::Button("Audio"))
			{
				showAudioWindow = !showAudioWindow;
			}

			if (ImGui::Button("Entities"))
			{
				showEntityList = !showEntityList;
			}
			ImGui::SameLine();
			if (ImGui::Button("Materials"))
			{
				showMaterialList = !showMaterialList;
			}
			ImGui::SameLine();
			if (ImGui::Button("Lighting"))
			{
				showLightWindow = !showLightWindow;
			}

			if (ImGui::Button("Matrix Calc"))
			{
				showMatrixCalculator = !showMatrixCalculator;
			}
		}
		ImGui::End();
	}

	// Log
	ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2{ 100, 30 });
	if (showLog)
	{
		if (ImGui::Begin("Log", &showLog))
		{
			if (ImGui::Button(stopLog ? ICON_PLAY_FILL : ICON_STOP_FILL))
			{
				if (!stopLog)
				{
					LOG("doot");
					WARN("Log paused...");
				}
				stopLog = !stopLog;
				if (!stopLog)
				{
					WARN("Log resumed...");
				}
			}
			ImGui::SameLine();
			ImGui::Checkbox("Scroll to bottom", &scrollLog);

			ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
			if (EngineLog::g_debugLog != nullptr) EngineLog::g_debugLog->DrawLogText();
			if (scrollLog)
			{
				ImGui::SetScrollHereY();
			}
			ImGui::PopStyleVar();
			ImGui::EndChild();
		}

		ImGui::End();
	}
	ImGui::PopStyleVar();

	// Graphics
	if (showPostProcessWindow)
	{
		if (ImGui::Begin("Graphics", &showPostProcessWindow))
		{
			ImGui::SliderFloat("Contrast", &contrast, 0., 3., "%.2f");
			ImGui::SliderFloat("Brightness", &brightness, -1., 1., "%.2f");
			ImGui::SliderFloat("Saturation", &saturation, 0., 3., "%.2f");
			ImGui::SliderFloat("Fog", &fog, 0., 3., "%.2f");
			ImGui::ColorEdit3("Clear Color", &baseClearColor.m128_f32[0]);
			ImGui::ColorEdit3("Fog Color", &fogColor.m128_f32[0]);
			ImGui::Separator();

			WindowMode currentWindowMode = engine.m_windowMode;
			if (ImGui::Combo("Window Mode", (int*)&currentWindowMode, "Fullscreen\0Borderless\0Windowed\0\0"))
			{
				engine.m_wantedWindowMode = currentWindowMode;
			}
			ImGui::Checkbox("VSync", &engine.m_useVsync);
			ImGui::Checkbox("MSAA", &engine.m_msaaEnabled);
			ImGui::Checkbox("Render Texture", &engine.m_renderTextureEnabled);
			ImGui::Text("Ray Tracing Support: %s", engine.m_raytracingSupport ? "yes" : "no");
			ImGui::BeginDisabled(!engine.m_raytracingSupport);
			ImGui::Checkbox("Ray Tracing", &engine.m_raytracingEnabled);
			ImGui::EndDisabled();

			ImGui::Separator();
			ImGui::Checkbox("Show Physics", &renderPhysics);
		}
		ImGui::End();
	}

	// Audio
	if (showAudioWindow)
	{
		if (ImGui::Begin("Audio", &showAudioWindow))
		{
			float volume = 0.f;
			engine.m_audioMasteringVoice->GetVolume(&volume);
			if (ImGui::SliderFloat("Global volume", &volume, 0., 1., "%.2f"))
			{
				engine.m_audioMasteringVoice->SetVolume(volume);
			}
		}
		ImGui::End();
	}

	// Movement
	if (showMovementWindow)
	{
		if (ImGui::Begin("Movement", &showMovementWindow))
		{
			ImGui::Checkbox("Autojump", &playerMovement.movementSettings->autojump);
			ImGui::SameLine();

			ImVec2 cursorPos = ImGui::GetCursorScreenPos();
			float playerDisplaySpeed = 0.f;// XMVectorGetX(XMVector3Length(XMVectorSetY(XMVectorFromPhysics(playerEntity->rigidBody->getLinearVelocity()), 0.)));
			ImGui::GetWindowDrawList()->AddLine({ cursorPos.x, cursorPos.y + 10.f }, { cursorPos.x + (playerDisplaySpeed * 30.f / playerMovement.movementSettings->playerMaxSpeed), cursorPos.y + 10.f }, ImColor(.1f, .2f, .9f), 10.f);
			ImGui::SetCursorPosX(cursorPos.x + 35.f);
			ImGui::Text("Vel: %.1f", playerDisplaySpeed);

			ImGui::Text("Pitch");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80.f);
			ImGui::DragFloat("##pitch", &playerPitch, .05f, -XMConvertToRadians(80.f), XMConvertToRadians(80.f), "%.1f");
			ImGui::SameLine();
			ImGui::Text("Yaw");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80.f);
			ImGui::DragFloat("##yaw", &playerYaw, .05f, -XMConvertToRadians(360.f), XMConvertToRadians(360.f), "%.1f");

			ImGui::SliderFloat("Acceleration", &playerMovement.movementSettings->playerAcceleration, 1., 200., "%.0f");
			ImGui::SliderFloat("Friction", &playerMovement.movementSettings->playerFriction, 1., 200., "%.0f");
			ImGui::SliderFloat("Gravity", &playerMovement.movementSettings->playerGravity, 1., 100., "%.0f");
			ImGui::SliderFloat("Max Speed", &playerMovement.movementSettings->playerMaxSpeed, 1., 100., "%.0f");
			ImGui::SliderFloat("Jump Strength", &playerMovement.movementSettings->playerJumpStrength, 1., 1000., "%.0f");
			ImGui::SliderFloat("Jump Buffer Duration", &playerMovement.jumpBufferDuration, 0.01, 1., "%.2f");
		}
		ImGui::End();
	}

	// Entity list
	gizmo.root->SetActive(false);
	if (showEntityList)
	{
		if (ImGui::Begin("Entity List", &showEntityList))
		{
			ImGui::Checkbox("Show Inactive Entities", &showInactiveEntities);
			ImGui::Checkbox("Gizmo Space Local", &gizmo.gizmoLocal);

			gizmo.editElement = nullptr;

			for (Entity& entity : entityArena)
			{
				int offset = &entity - (Entity*)entityArena.base;
				ImGui::PushID(&entity);

				const char* icon = entity.IsActive() ? ICON_CHECK_FILL : "";
				std::string entityTitle = std::format("{} [{}] {}###{}", entity.name, offset, icon, reinterpret_cast<void*>(&entity));
				if ((showInactiveEntities || entity.IsActive()) && ImGui::CollapsingHeader(entityTitle.c_str()))
				{
					ImGui::Text("NAME");
					ImGui::InputText("##entityname", entity.name.str, FixedStr::SIZE);

					ImGui::Separator();


					ImGui::Text(std::format("CHILDREN ({})", entity.children.size).c_str());

					for (int i = 0; i < entity.children.size; i++)
					{
						if (ImGui::SmallButton(std::format("{}###{}", ICON_CLOSE_FILL, i).c_str()))
						{
							entity.RemoveChild(entity.children[i], true);
							i--;
							continue;
						}

						ImGui::SameLine();
						if (entity.children[i].Get() == nullptr) continue;
						Entity* child = entity.children[i].Get();
						ImGui::Text(std::format("{} [{}]", child->name, (child - (Entity*)entityArena.base)).c_str());
					}

					if (ImGui::Button("Add"))
					{
						entity.AddChild((Entity*)entityArena.base + newChildId, true);
					}
					ImGui::SameLine();
					ImGui::PushItemWidth(100);
					ImGui::InputInt("", &newChildId);
					ImGui::PopItemWidth();


					ImGui::Separator();

					bool activeState = entity.IsActive();
					if (ImGui::Checkbox("Active", &activeState))
					{
						entity.SetActive(activeState);
					}
					if (entity.isRendered)
					{
						ImGui::Checkbox("Visible", &entity.GetData().visible);
					}
					else
					{
						bool never = false;
						ImGui::BeginDisabled();
						ImGui::Checkbox("Visible", &never);
						ImGui::EndDisabled();
					}

					ImGui::Separator();

					ImGui::Text("TRANSFORM");
					XMVECTOR pos = entity.GetLocalPosition();
					if (ImGui::DragFloat3("Position", &pos.m128_f32[0], SLIDER_SPEED, SLIDER_MIN, SLIDER_MAX, "%.1f", ImGuiSliderFlags_NoRoundToFormat))
					{
						entity.SetLocalPosition(pos);
					}

					XMVECTOR rot = QuaternionToEuler(entity.GetLocalRotation());
					if (ImGui::DragFloat3("Rotation", &rot.m128_f32[0], SLIDER_SPEED, SLIDER_MIN, SLIDER_MAX, "%.1f", ImGuiSliderFlags_NoRoundToFormat))
					{
						entity.SetLocalRotation(XMQuaternionRotationRollPitchYaw(XMVectorGetX(rot), XMVectorGetY(rot), XMVectorGetZ(rot)));
					}

					XMVECTOR scale = entity.GetLocalScale();
					if (ImGui::DragFloat3("Scale", &scale.m128_f32[0], SLIDER_SPEED, SLIDER_MIN, SLIDER_MAX, "%.1f", ImGuiSliderFlags_NoRoundToFormat))
					{
						entity.SetLocalScale(scale);
					}

					ImGui::BeginTabBar("Transform");
					if (ImGui::BeginTabItem("Local"))
					{
						DisplayMatrix(entity.localMatrix.matrix);
						ImGui::EndTabItem();
					}
					if (ImGui::BeginTabItem("Global"))
					{
						DisplayMatrix(entity.worldMatrix.matrix);
						ImGui::EndTabItem();
					}
					ImGui::EndTabBar();

					if (entity.rigidBody != nullptr)
					{
						ImGui::Separator();
						const char* bodyTypeIcon = nullptr;

						if (entity.rigidBody->isStaticOrKinematicObject())
						{
							if (entity.rigidBody->isStaticObject())
							{
								bodyTypeIcon = ICON_HOME_2_LINE;
							}
							else
							{
								bodyTypeIcon = ICON_DRAG_MOVE_2_LINE;
							}
						}
						else
						{
							bodyTypeIcon = ICON_SEND_PLANE_LINE;
						}

						ImGui::Text("%s RIGIDBODY %s", bodyTypeIcon, entity.rigidBody->isActive() ? "" : ICON_ZZZ_FILL);
						ImGui::Text("In World: %s", entity.rigidBody->isInWorld() ? "yes" : "no");
						ImGui::Text("Velocity: %.1f %.1f %.1f", SPLIT_V3_BT(entity.rigidBody->getLinearVelocity()));
						ImGui::SameLine();
						ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x / 2.f);
						ImGui::Text("Angular Velocity: %.1f %.1f %.1f", SPLIT_V3_BT(entity.rigidBody->getAngularVelocity()));
						ImGui::Text("Inertia: %.1f %.1f %.1f", SPLIT_V3_BT(entity.rigidBody->getLocalInertia()));
						ImGui::SameLine();
						ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x / 2.f);
						ImGui::Text("Gravity: %.1f %.1f %.1f", SPLIT_V3_BT(entity.rigidBody->getGravity()));

						float mass = entity.rigidBody->getMass();
						if (ImGui::SliderFloat("Mass", &mass, 0.f, 1000.f, "%.1f"))
						{
							entity.rigidBody->setMassProps(mass, entity.rigidBody->getLocalInertia());
						}

						float friction = entity.rigidBody->getFriction();
						if (ImGui::SliderFloat("Friction", &friction, 0.f, 1.f, "%.1f"))
						{
							entity.rigidBody->setFriction(friction);
						}
						
						btVector3 rotationLock = entity.rigidBody->getAngularFactor();
						bool xLocked = rotationLock.x() < 0.01f;
						bool yLocked = rotationLock.y() < 0.01f;
						bool zLocked = rotationLock.z() < 0.01f;
						ImGui::Text("Lock Rotation: ");
						ImGui::SameLine();
						ImGui::Checkbox("X##rotationlockx", &xLocked);
						ImGui::SameLine();
						ImGui::Checkbox("Y##rotationlocky", &yLocked);
						ImGui::SameLine();
						ImGui::Checkbox("Z##rotationlockz", &zLocked);
						rotationLock.setX(xLocked ? 0.f : 1.f);
						rotationLock.setY(yLocked ? 0.f : 1.f);
						rotationLock.setZ(zLocked ? 0.f : 1.f);
						entity.rigidBody->setAngularFactor(rotationLock);

						ImGui::DragFloat3("##ApplyForce", &physicsForceDebug.m128_f32[0], SLIDER_SPEED, SLIDER_MIN, SLIDER_MAX, "%.1f");
						ImGui::SameLine();
						if (ImGui::Button("Apply Force"))
						{
							entity.rigidBody->applyCentralForce(ToBulletVec3(physicsForceDebug));
						}

						ImGui::DragFloat3("##ApplyTorque", &physicsTorqueDebug.m128_f32[0], SLIDER_SPEED, SLIDER_MIN, SLIDER_MAX, "%.1f");
						ImGui::SameLine();
						if (ImGui::Button("Apply Torque"))
						{
							entity.rigidBody->applyTorque(ToBulletVec3(physicsTorqueDebug));
						}

						const char* shapeTypeIcon = nullptr;
						switch (entity.rigidBody->getCollisionShape()->getShapeType())
						{
							case BOX_SHAPE_PROXYTYPE:
								shapeTypeIcon = ICON_CHECKBOX_BLANK_LINE;
								break;
							case SPHERE_SHAPE_PROXYTYPE:
								shapeTypeIcon = ICON_CHECKBOX_BLANK_CIRCLE_LINE;
								break;
							case CAPSULE_SHAPE_PROXYTYPE:
								shapeTypeIcon = ICON_CAPSULE_LINE;
								break;
							default:
								shapeTypeIcon = ICON_QUESTION_LINE;
								break;
						}
						ImGui::Text("COLLISION SHAPE %s", shapeTypeIcon);
					}

					if (entity.isSkinnedRoot && entity.transformHierachy->nodeCount > 0)
					{
						TransformHierachy* hierachy = entity.transformHierachy;

						ImGui::Separator();
						ImGui::Text("ANIMATIONS");

						for (int animIdx = 0; animIdx < hierachy->animationCount; animIdx++)
						{
							TransformAnimation& animation = hierachy->animations[animIdx];

							if (ImGui::SmallButton(std::format("{}###{}", animation.active ? ICON_PAUSE_FILL : ICON_PLAY_FILL, animation.name.c_str()).c_str()))
							{
								animation.active = !animation.active;
							}
							ImGui::SameLine();
							ImGui::Text(animation.name.c_str());
						}

						ImGui::Separator();

						std::string nodeTitle = std::format("BONES: {}", entity.transformHierachy->nodeCount);
						if (ImGui::TreeNodeEx(nodeTitle.c_str()))
						{
							DisplayTransformNode(entity.transformHierachy->root);
							ImGui::TreePop();
						}
					}

					gizmo.root->SetLocalPosition(entity.worldMatrix.translation);
					gizmo.root->SetLocalRotation(entity.worldMatrix.rotation);
					gizmo.root->SetActive(gizmo.editMode);
					gizmo.editElement = &entity;
				}
				ImGui::PopID();
			}

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 20.f);
			
			ImGui::SameLine();

			ImGui::Combo("##entitymaterial", &newEntityMaterialIndex, [](void* data, int idx, const char** out_text)
				{
					*out_text = ((EngineCore*)data)->m_materials[idx].name.c_str();
					return true;
				}, &engine, engine.m_materials.size);
		}
		ImGui::End();
	}

	// Materials
	if (showMaterialList)
	{
		if (ImGui::Begin("Materials", &showMaterialList))
		{
			for (MaterialData& mat : engine.m_materials)
			{
				ImGui::PushID(&mat);
				if (ImGui::CollapsingHeader(mat.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				{
					float availableX = ImGui::GetContentRegionAvail().x;
					ImGui::Text("Entities: %d", mat.entities.size);
					ImGui::SameLine();
					ImGui::SetCursorPosX(availableX / 2.f);

					for (Texture* tex : mat.textures)
					{
						ImGui::PushID(tex);
						if (ImGui::TreeNode(tex->name.empty() ? "texture" : tex->name.c_str()))
						{
							auto desc = tex->buffer->GetDesc();
							float aspectRatio = static_cast<float>(desc.Width) / static_cast<float>(desc.Height);
							engine.m_imgui.UIImage(engine.m_device, tex, { 128 , 128 * aspectRatio });
							ImGui::TreePop();
						}
						ImGui::PopID();
					}

					ImGui::NewLine();
				}
				ImGui::PopID();
			}
		}

		ImGui::End();
	}

	// Lighting
	if (showLightWindow)
	{
		if (ImGui::Begin("Lighting", &showLightWindow))
		{
			ImGui::Checkbox("Show Light Space", &showLightSpaceDebug);
			ImGui::Checkbox("Show Light Position", &showLightPosition);

			// light position
			ImGui::Text("Light Position: (%.1f %.1f %.1f)", light.position.m128_f32[0], light.position.m128_f32[1], light.position.m128_f32[2]);
			ImGui::Text("Light Rotation: (%.1f %.1f %.1f %.1f)", light.rotation.m128_f32[0], light.rotation.m128_f32[1], light.rotation.m128_f32[2], light.rotation.m128_f32[3]);
		}
		ImGui::End();
	}

	if (showMatrixCalculator)
	{
		if (ImGui::Begin("Matrix Calc", &showMatrixCalculator))
		{
			if (ImGui::BeginCombo("Camera", matrixCalcSelectedCam == nullptr ? "-" : matrixCalcSelectedCam->name.str))
			{
				if (ImGui::Selectable("-", matrixCalcSelectedCam == nullptr))
				{
					matrixCalcSelectedCam = nullptr;
					matrixCalcViewMatrix = XMMatrixIdentity();
					matrixCalcProjectionMatrix = XMMatrixIdentity();
				}

				for (CameraData& cam : engine.m_cameras)
				{
					bool isSelected = matrixCalcSelectedCam == &cam;
					if (ImGui::Selectable(cam.name.str, isSelected))
					{
						matrixCalcSelectedCam = &cam;
					}
					if (isSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
			ImGui::Separator();

			if (matrixCalcSelectedCam != nullptr)
			{
				matrixCalcViewMatrix = XMMatrixTranspose(matrixCalcSelectedCam->constantBuffer.data.cameraView);
				matrixCalcProjectionMatrix = XMMatrixTranspose(matrixCalcSelectedCam->constantBuffer.data.cameraProjection);
			}

			ImGui::Text("Input Vector");
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputFloat4("##calcipnut", &matrixCalcInputVec.m128_f32[0]);
			ImGui::Separator();

			ImGui::Text("Model");
			InputMatrix(matrixCalcModelMatrix, "Model");
			XMVECTOR worldResult = XMVector3Transform(matrixCalcInputVec, matrixCalcModelMatrix);
			ImGui::Text("Result (World): %.1f %.1f %.1f %.1f", SPLIT_V4(worldResult));
			ImGui::Separator();

			ImGui::Text("View");
			InputMatrix(matrixCalcViewMatrix, "View");
			XMVECTOR viewResult = XMVector3Transform(worldResult, matrixCalcViewMatrix);
			ImGui::Text("Result (View): %.1f %.1f %.1f %.1f", SPLIT_V4(viewResult));
			ImGui::Separator();

			ImGui::Text("Projection");
			InputMatrix(matrixCalcProjectionMatrix, "Projection");
			XMVECTOR projectionResult = XMVector3Transform(viewResult, matrixCalcProjectionMatrix);
			ImGui::Text("Result (Projection): %.1f %.1f %.1f %.1f", SPLIT_V4(projectionResult));

			XMVECTOR ndcResult = projectionResult / projectionResult.m128_f32[3];
			ImGui::Text("Result (NDC): %.1f %.1f %.1f %.1f", SPLIT_V4(ndcResult));
		}
		ImGui::End();
	}

	engine.EndProfile("Game UI");

	// Profiler
	if (!pauseProfiler)
	{
		profilerWindow.cpuGraph.LoadFrameData(engine.m_profilerTaskData.data(), engine.m_profilerTaskData.size());
		engine.m_profilerTaskData.clear();
		engine.m_profilerTasks.clear();
	}
	if (showProfiler)
	{
		profilerWindow.Render(&showProfiler);
	}
}

void LoadUIStyle()
{
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowPadding = { 6.f, 8.f };
	style.FramePadding = { 8.f, 4.f };
	style.ItemSpacing = { 4.f, 4.f };
	style.WindowRounding = 4.f;
	style.GrabRounding = 2.f;

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.07f, 0.09f, 0.99f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.04f, 0.07f, 1.00f);
	colors[ImGuiCol_Border] = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.02f, 0.01f, 0.01f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.98f, 0.36f, 0.36f, 0.40f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.60f, 0.41f, 0.41f, 0.67f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.48f, 0.16f, 0.16f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.02f, 0.02f, 0.02f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.06f, 0.05f, 0.06f, 0.99f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.98f, 0.61f, 0.26f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.88f, 0.38f, 0.24f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.98f, 0.41f, 0.26f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.04f, 0.01f, 0.03f, 0.73f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.14f, 0.23f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.47f, 0.23f, 0.39f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.46f, 0.19f, 0.12f, 0.67f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.69f, 0.29f, 0.19f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.98f, 0.41f, 0.26f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.50f, 0.45f, 0.43f, 0.50f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.75f, 0.24f, 0.10f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.75f, 0.24f, 0.10f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.98f, 0.41f, 0.26f, 0.20f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.98f, 0.41f, 0.26f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.98f, 0.41f, 0.26f, 0.95f);
	colors[ImGuiCol_Tab] = ImVec4(0.09f, 0.02f, 0.07f, 0.86f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.15f, 0.24f, 1.00f);
	colors[ImGuiCol_TabActive] = ImVec4(0.37f, 0.14f, 0.29f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.09f, 0.07f, 0.97f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.42f, 0.20f, 0.14f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}