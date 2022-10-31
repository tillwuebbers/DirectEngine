#pragma once

#include "imgui.h"
#include <string>

const int LOG_SIZE = 1024;

struct LogMessage
{
	ImColor color;
	std::string message;
};

class RingLog
{
public:
	void DrawLogText();
	void AppendToLog(const std::string& message, ImU32 color);
	void Log(const std::string& message);
	void Warn(const std::string& message);
	void Error(const std::string& message);

private:
	LogMessage messages[LOG_SIZE] = {};
	int debugLogIndex = 0;
	int debugLogCount = 0;
};
