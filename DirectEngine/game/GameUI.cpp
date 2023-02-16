#include "Game.h"
#include "remixicon.h"
#include "../core/UI.h"

#include <numeric>

#define SLIDER_SPEED 0.005f
#define SLIDER_MIN (-1000.f)
#define SLIDER_MAX 1000.f
#define SPLIT_V3(v) v.m128_f32[0], v.m128_f32[1], v.m128_f32[2]
#define SPLIT_V4(v) v.m128_f32[0], v.m128_f32[1], v.m128_f32[2], v.m128_f32[3]

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

void Game::DrawUI(EngineCore& engine)
{
	engine.BeginProfile("Game UI", ImColor::HSV(.75f, 1.f, .75f));
	if (showDemoWindow) ImGui::ShowDemoWindow(&showDemoWindow);

	// Escape Menu
	if (showEscMenu)
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
	if (showDebugUI)
	{
		std::string title = std::format("{} Debug", ICON_BUG_FILL);

		ImGui::SetNextWindowPos(ImVec2{ 0, 0 });
		if (ImGui::Begin(title.c_str(), &showDebugUI, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize))
		{
			switch (engine.m_windowMode)
			{
			case WindowMode::Borderless:
				ImGui::Text("Window mode: Borderless");
				break;
			case WindowMode::Fullscreen:
				ImGui::Text("Window mode: Fullscreen");
				break;
			case WindowMode::Windowed:
				ImGui::Text("Window mode: Window");
				break;
			}

			ImGui::NewLine();
			ImGui::Text("Camera Position: %.1f %.1f %.1f", SPLIT_V3(camera.position));
			ImGui::Text("Camera Rotation: %.1f %.1f", playerPitch / XM_2PI * 360.f, playerYaw / XM_2PI * 360.f);

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

			if (ImGui::Checkbox("Borderless", &engine.m_wantBorderless))
			{
				if (engine.m_windowMode == WindowMode::Fullscreen)
				{
					engine.m_wantedWindowMode = WindowMode::Borderless;
				}
				else if (engine.m_windowMode == WindowMode::Borderless)
				{
					engine.m_wantedWindowMode = WindowMode::Fullscreen;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Toggle Fullscreen"))
			{
				engine.ToggleWindowMode();
			}
			if (ImGui::Checkbox("VSync", &engine.m_useVsync))
			{

			}

			ImGui::NewLine();

			if (ImGui::Button("ImGui Demo"))
			{
				showDemoWindow = !showDemoWindow;
			}
			ImGui::SameLine();
			if (ImGui::Button("Debug Image"))
			{
				showDebugImage = !showDebugImage;
			}

			if (ImGui::Button("Log"))
			{
				showLog = !showLog;
			}
			ImGui::SameLine();
			if (ImGui::Button("Profiler"))
			{
				showProfiler = !showProfiler;
			}
			ImGui::SameLine();
			if (ImGui::Button("Movement"))
			{
				showMovementWindow = !showMovementWindow;
			}

			if (ImGui::Button("Post Processing"))
			{
				showPostProcessWindow = !showPostProcessWindow;
			}
			ImGui::SameLine();
			if (ImGui::Button("Audio"))
			{
				showAudioWindow = !showAudioWindow;
			}

			if (ImGui::Button("Entity List"))
			{
				showEntityList = !showEntityList;
			}
			ImGui::SameLine();
			if (ImGui::Button("Lighting"))
			{
				showLightWindow = !showLightWindow;
			}

			ImGui::NewLine();

			ImGui::Checkbox("Show Bounds", &engine.renderAABB);
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
					Warn("Log paused...");
				}
				stopLog = !stopLog;
				if (!stopLog)
				{
					Warn("Log resumed...");
				}
			}
			ImGui::SameLine();
			ImGui::Checkbox("Scroll to bottom", &scrollLog);

			ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
			debugLog.DrawLogText();
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

	// Debug Image
	if (showDebugImage)
	{
		if (ImGui::Begin("Debug Image", &showDebugImage))
		{
			ImVec2 availableSize = ImGui::GetContentRegionAvail();
			float minSize = std::min(availableSize.x, availableSize.y);
			DrawDebugImage({ minSize, minSize });
		}
		ImGui::End();
	}

	// Post Processing
	if (showPostProcessWindow)
	{
		if (ImGui::Begin("Post Processing", &showPostProcessWindow))
		{
			ImGui::SliderFloat("Contrast", &contrast, 0., 3., "%.2f");
			ImGui::SliderFloat("Brightness", &brightness, -1., 1., "%.2f");
			ImGui::SliderFloat("Saturation", &saturation, 0., 3., "%.2f");
			ImGui::SliderFloat("Fog", &fog, 0., 3., "%.2f");
			ImGui::ColorEdit3("Clear Color", &baseClearColor.m128_f32[0]);
			ImGui::ColorEdit3("Fog Color", &fogColor.m128_f32[0]);
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
			ImGui::Checkbox("Noclip", &noclip);
			ImGui::SameLine();
			ImGui::Checkbox("Autojump", &autojump);
			ImGui::SameLine();

			ImVec2 cursorPos = ImGui::GetCursorScreenPos();
			float playerDisplaySpeed = XMVectorGetX(XMVector3Length(XMVectorSetY(playerVelocity, 0.)));
			ImGui::GetWindowDrawList()->AddLine({ cursorPos.x, cursorPos.y + 10.f }, {cursorPos.x + (playerDisplaySpeed * 30.f / playerMaxSpeed), cursorPos.y + 10.f }, ImColor(.1f, .2f, .9f), 10.f);
			ImGui::SetCursorPosX(cursorPos.x + 35.f);
			ImGui::Text("Vel: %.1f", playerDisplaySpeed);

			ImGui::SliderFloat("Acceleration", &playerAcceleration, 1., 200., "%.0f");
			ImGui::SliderFloat("Friction", &playerFriction, 1., 200., "%.0f");
			ImGui::SliderFloat("Gravity", &playerGravity, 1., 100., "%.0f");
			ImGui::SliderFloat("Max Speed", &playerMaxSpeed, 1., 100., "%.0f");
			ImGui::SliderFloat("Jump Strength", &playerJumpStrength, 1., 1000., "%.0f");
			ImGui::SliderFloat("Jump Buffer Duration", &jumpBufferDuration, 0.01, 1., "%.2f");
		}
		ImGui::End();
	}
	
	// Entity list
	if (showEntityList)
	{
		if (ImGui::Begin("Entity List", &showEntityList))
		{
			ImGui::Checkbox("Show Inactive Entities", &showInactiveEntities);

			for (Entity* entity = (Entity*)entityArena.base; entity != (Entity*)(entityArena.base + entityArena.used); entity++)
			{
				int offset = entity - (Entity*)entityArena.base;
				
				std::string entityTitle = std::format("{} [{}] ", entity->name, offset);
				if (entity->isActive) entityTitle.append(ICON_CHECK_FILL);
				ImGui::PushID(entity);
				if ((showInactiveEntities || entity->isActive) && ImGui::CollapsingHeader(entityTitle.c_str()))
				{
					ImGui::Text(std::format("Children ({})", entity->childCount).c_str());
					ImGui::SameLine(0, 20);
					if (ImGui::Button("Add"))
					{
						entity->AddChild((Entity*)entityArena.base + newChildId);
					}
					ImGui::SameLine();
					ImGui::PushItemWidth(100);
					ImGui::InputInt("", &newChildId);
					ImGui::PopItemWidth();

					for (int i = 0; i < entity->childCount; i++)
					{
						if (ImGui::SmallButton(std::format("{}###{}", ICON_CLOSE_FILL, i).c_str()))
						{
							entity->RemoveChild(entity->children[i]);
							i--;
							continue;
						}
						
						ImGui::SameLine();
						ImGui::Text(std::format("{} [{}]", entity->children[i]->name, (entity->children[i] - (Entity*)entityArena.base)).c_str());
					}
					

					ImGui::Separator();

					ImGui::Checkbox("Active", &entity->isActive);
					if (entity->isRendered)
					{
						ImGui::Checkbox("Visible", &entity->GetData().visible);
					}
					else
					{
						bool never = false;
						ImGui::BeginDisabled();
						ImGui::Checkbox("Visible", &never);
						ImGui::EndDisabled();
					}
					ImGui::Checkbox("Shadow Bounds", &entity->checkForShadowBounds);
					std::vector<std::string> attributes{};
					if (entity->isEnemy) attributes.push_back("enemy");
					if (entity->isProjectile) attributes.push_back("projectile");
					std::string attributesString = std::accumulate(attributes.begin(), attributes.end(), std::string(), [](std::string a, std::string b) { return a + (a.empty() ? "" : ", ") + b; });
					ImGui::Text("Attributes: %s", attributesString.c_str());

					ImGui::Separator();

					ImGui::DragFloat3("Position", &entity->position.m128_f32[0], SLIDER_SPEED, SLIDER_MIN, SLIDER_MAX, "%.1f", ImGuiSliderFlags_NoRoundToFormat);
					ImGui::DragFloat4("Rotation", &entity->rotation.m128_f32[0], SLIDER_SPEED, SLIDER_MIN, SLIDER_MAX, "%.1f", ImGuiSliderFlags_NoRoundToFormat);
					ImGui::DragFloat3("Scale", &entity->scale.m128_f32[0], SLIDER_SPEED, SLIDER_MIN, SLIDER_MAX, "%.1f", ImGuiSliderFlags_NoRoundToFormat);

					ImGui::Separator();

					ImGui::BeginTabBar("Transform");
					if (ImGui::BeginTabItem("Local"))
					{
						DisplayMatrix(entity->localMatrix);
						ImGui::EndTabItem();
					}
					if (ImGui::BeginTabItem("Global"))
					{
						DisplayMatrix(entity->worldMatrix);
						ImGui::EndTabItem();
					}
					ImGui::EndTabBar();


					ImGui::Separator();

					ImGui::DragFloat3("Bounding Center", &entity->aabbLocalPosition.m128_f32[0], SLIDER_SPEED, SLIDER_MIN, SLIDER_MAX, "%.1f", ImGuiSliderFlags_NoRoundToFormat);
					ImGui::DragFloat3("Bounding Extent", &entity->aabbLocalSize.m128_f32[0], SLIDER_SPEED, SLIDER_MIN, SLIDER_MAX, "%.1f", ImGuiSliderFlags_NoRoundToFormat);

					if (entity->isSkinnedRoot && entity->transformHierachy->nodeCount > 0)
					{
						ImGui::Separator();
						ImGui::Checkbox("Play Animation", &entity->transformHierachy->animationActive);
						ImGui::SameLine();
						ImGui::PushItemWidth(100);
						InputSizeT("Animation Index", &entity->transformHierachy->animationIndex);
						ImGui::PopItemWidth();

						std::string nodeTitle = std::format("Bone Count: {}", entity->transformHierachy->nodeCount);
						if (ImGui::TreeNodeEx(nodeTitle.c_str()))
						{
							DisplayTransformNode(entity->transformHierachy->root);
							ImGui::TreePop();
						}
					}
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
			if (ImGui::Checkbox("Show Light Position", &showLightPosition))
			{
				lightDebugEntity->GetData().visible = showLightPosition;
			}

			// light position
			ImGui::Text("Light Position: (%.1f %.1f %.1f)", light.position.m128_f32[0], light.position.m128_f32[1], light.position.m128_f32[2]);
			ImGui::Text("Light Rotation: (%.1f %.1f %.1f %.1f)", light.rotation.m128_f32[0], light.rotation.m128_f32[1], light.rotation.m128_f32[2], light.rotation.m128_f32[3]);
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