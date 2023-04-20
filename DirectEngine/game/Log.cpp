#include "Log.h"

namespace GameLog
{
	RingLog* g_debugLog = nullptr;

	void RingLog::AppendToLog(const std::string& message, ImU32 color)
	{
		messages[debugLogIndex].color = color;
		messages[debugLogIndex].message = message;
		debugLogIndex = (debugLogIndex + 1) % LOG_SIZE;

		if (debugLogCount < LOG_SIZE)
		{
			debugLogCount++;
		}
	}

	void RingLog::Log(const std::string& message)
	{
		AppendToLog(message, IM_COL32_WHITE);
	}

	void RingLog::Warn(const std::string& message)
	{
		AppendToLog(message, ImColor::HSV(.09f, .9f, 1.f));
	}

	void RingLog::Error(const std::string& message)
	{
		AppendToLog(message, ImColor::HSV(.0f, .9f, 1.f));
	}

	void RingLog::DrawLogText()
	{
		for (int i = 0; i < debugLogCount; i++)
		{
			int realIndex = (debugLogIndex - debugLogCount + i + LOG_SIZE) % LOG_SIZE;
			LogMessage& message = messages[realIndex];
			ImGui::PushStyleColor(ImGuiCol_Text, message.color.Value);
			ImGui::Text(message.message.c_str());
			ImGui::PopStyleColor();
		}
	}

	void GlobalLog(LogMode mode, const std::string& message)
	{
		const char* logPrefix = " INFO: ";
		const char* warnPrefix = " WARN: ";
		const char* errorPrefix = "ERROR: ";

		const char* selectedPrefix;
		switch (mode)
		{
		case GameLog::LogMode::Warn:
			selectedPrefix = warnPrefix;
			break;
		case GameLog::LogMode::Error:
			selectedPrefix = errorPrefix;
			break;
		default:
			selectedPrefix = logPrefix;
			break;
		}

		std::cout << selectedPrefix << message << std::endl;

		// TODO: log to file

		assert(g_debugLog != nullptr);
		if (g_debugLog != nullptr)
		{
			switch (mode)
			{
			case GameLog::LogMode::Warn:
				g_debugLog->Warn(message);
				break;
			case GameLog::LogMode::Error:
				g_debugLog->Error(message);
				break;
			default:
				g_debugLog->Log(message);
				break;
			}
		}
	}
}
