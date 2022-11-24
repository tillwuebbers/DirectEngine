#include "Game.h"
#include "remixicon.h"
#include "../core/UI.h"

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
		}
		ImGui::End();
	}
	engine.EndProfile("Game UI");

	if (showMovementWindow)
	{
		if (ImGui::Begin("Movement", &showMovementWindow))
		{
			ImGui::Checkbox("Noclip", &noclip);
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