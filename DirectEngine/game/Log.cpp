#include "Log.h"

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