#pragma once
#include <string>

namespace adria::paths
{
	std::string MainDir();
	std::string ResourcesDir();
	std::string SavedDir();
	std::string ConfigDir();
	std::string ToolsDir();

	std::string FontsDir();
	std::string IconsDir();
	std::string ShaderDir();
	std::string TexturesDir();

	std::string LogDir();
	std::string ScreenshotsDir();
	std::string PixCapturesDir();
	std::string RenderGraphDir();
	std::string ShaderCacheDir();
	std::string ShaderPDBDir();
	std::string IniDir();
	std::string AftermathDir();
}