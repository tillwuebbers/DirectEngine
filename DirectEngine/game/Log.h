#pragma once

#include "../core/Constants.h"
#include "imgui.h"
#include <string>
#include <format>
#include <iostream>

namespace GameLog
{
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

	enum class LogMode
	{
		Log,
		Warn,
		Error,
	};

	extern RingLog* g_debugLog;

	void GlobalLog(LogMode mode, const std::string& message);

	template <typename... Args>
	void GlobalLog(LogMode mode, std::format_string<Args...> fmt, Args&&... args)
	{
		GlobalLog(mode, std::format(fmt, args...));
	}
}

#define LOG(...) GameLog::GlobalLog(GameLog::LogMode::Log, __VA_ARGS__)
#define WARN(...) GameLog::GlobalLog(GameLog::LogMode::Warn, __VA_ARGS__)
#define ERR(...) GameLog::GlobalLog(GameLog::LogMode::Error, __VA_ARGS__)
