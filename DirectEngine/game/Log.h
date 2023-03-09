#pragma once

#include "imgui.h"
#include <string>
#include <format>

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

	template <typename... Args>
	void Log(std::format_string<Args...> fmt, Args&&... args)
	{
		Log(std::format(fmt, args...));
	}

	template <typename... Args>
	void Warn(std::format_string<Args...> fmt, Args&&... args)
	{
		Warn(std::format(fmt, args...));
	}

	template <typename... Args>
	void Error(std::format_string<Args...> fmt, Args&&... args)
	{
		Error(std::format(fmt, args...));
	}

private:
	LogMessage messages[LOG_SIZE] = {};
	int debugLogIndex = 0;
	int debugLogCount = 0;
};
