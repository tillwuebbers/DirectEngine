#include "Game.h"
#include "remixicon.h"

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

		ImGui::SetNextWindowSize(ImVec2(engine.m_width, engine.m_height));
		ImGui::SetNextWindowPos(ImVec2());
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(hAlign, vAlign));
		ImGui::Begin("Pause Menu", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

		if (ImGui::Button("Continue", buttonSize))
		{
			showEscMenu = false;
		}
		if (ImGui::Button("Debug Menu", buttonSize))
		{
			showDebugUI = true;
			showEscMenu = false;
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

			if (ImGui::Button("Show Demo"))
			{
				showDemoWindow = !showDemoWindow;
			}
			if (ImGui::Button("Show Log"))
			{
				showLog = !showLog;
			}
			ImGui::SameLine();
			if (ImGui::Button("Show Profiler"))
			{
				showProfiler = !showProfiler;
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
					Warn("Log paused...");
				}
				stopLog = !stopLog;
				if (!stopLog)
				{
					Warn("Log resumed...");
				}
			}
			ImGui::SameLine();

			ImGui::BeginChild("scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
			debugLog.DrawText();
			ImGui::PopStyleVar();
			ImGui::EndChild();
		}
		ImGui::End();
	}
	ImGui::PopStyleVar();

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