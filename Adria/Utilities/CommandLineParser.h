#pragma once
#include "../Core/Definitions.h"

namespace adria
{
	enum class ECmdLineArg : uint8
	{
		Invalid,
		Vsync,
		DebugLayer,
		GpuValidation,
		DredDebug,
		WindowMaximize,
		WindowWidth,
		WindowHeight,
		LogLevel,
		WindowTitle,
		SceneFile,
		LogFile
	};

	namespace CommandLineArgs
	{
		void Parse(wchar_t const* wide_cmd_line);
		bool GetBool(ECmdLineArg arg);
		int32 GetInt(ECmdLineArg arg);
		char const* GetString(ECmdLineArg arg);
	}
	
}