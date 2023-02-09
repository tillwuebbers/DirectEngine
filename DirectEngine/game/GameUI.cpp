#include "Game.h"
#include "remixicon.h"
#include "../core/UI.h"

#include <numeric>

std::string FormatVector3(XMVECTOR in)
{
	XMFLOAT3 data;
	XMStoreFloat3(&data, in);
	return std::format("{:.1f}, {:.1f}, {:.1f}", data.x, data.y, data.z);
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
			ImGui::Text(std::format("Camera Position: {}", FormatVector3(camera.position)).c_str());
			ImGui::Text(std::format("Camera pitch/yaw: {:.0f}, {:.0f}", playerPitch / XM_2PI * 360.f, playerYaw / XM_2PI * 360.f).c_str());

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
			ImGui::Checkbox("Show Bones", &engine.renderBones);
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

			ImGui::SliderFloat("Player Height", &playerHeight, 0.1, 5., "%.1f");
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
				EntityData& entityData = entity->GetData();
				
				std::string entityTitle = std::format("{} [{}] ", entity->name, offset);
				if (entity->isActive) entityTitle.append(ICON_CHECK_FILL);
				if ((showInactiveEntities || entity->isActive) && ImGui::TreeNodeEx(entity, 0, entityTitle.c_str()))
				{
					ImGui::Checkbox("Active", &entity->isActive);
					ImGui::Checkbox("Visible", &entity->GetData().visible);
					std::vector<std::string> attributes{};
					if (entity->isEnemy) attributes.push_back("enemy");
					if (entity->isProjectile) attributes.push_back("projectile");
					if (entity->isSpinning) attributes.push_back("speeen");
					std::string attributesString = std::accumulate(attributes.begin(), attributes.end(), std::string(), [](std::string a, std::string b) { return a + (a.empty() ? "" : ", ") + b; });
					ImGui::Text("Attributes: %s", attributesString.c_str());

					ImGui::InputFloat3("Position", &entity->position.m128_f32[0], "%.1f");
					ImGui::InputFloat4("Rotation", &entity->rotation.m128_f32[0], "%.1f");
					ImGui::InputFloat3("Scale", &entity->scale.m128_f32[0], "%.1f");

					ImGui::InputFloat3("Bounding Center", &entityData.aabbLocalPosition.m128_f32[0], "%.1f");
					ImGui::InputFloat3("Bounding Extent", &entityData.aabbLocalSize.m128_f32[0], "%.1f");
					if (entityData.boneCount > 0)
					{
						ImGui::Text("Bone Count: %d", entityData.boneCount);
						ImGui::SliderInt("Selected Bone", &boneDebugIndex, 0, entityData.boneCount);
					}
					ImGui::TreePop();
				}
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