#include "Paths.h"

namespace adria
{

	std::string paths::MainDir()
	{
		return "";
	}

	std::string paths::ConfigDir()
	{
		return MainDir();
	}
	std::string paths::ResourcesDir()
	{
		return MainDir() + "Resources/";
	}
	std::string paths::SavedDir()
	{
		return MainDir() + "Saved/";
	}
	std::string paths::ToolsDir()
	{
		return MainDir() + "Tools/";
	}

	std::string paths::FontsDir()
	{
		return ResourcesDir() + "Fonts/";
	}
	std::string paths::IconsDir()
	{
		return ResourcesDir() + "Icons/";
	}
	std::string paths::ShaderDir()
	{
		return ResourcesDir() + "NewShaders/";
	}
	std::string paths::TexturesDir()
	{
		return ResourcesDir() + "Textures/";
	}

	std::string paths::ScreenshotsDir()
	{
		return SavedDir() + "Screenshots/";
	}
	std::string paths::LogDir()
	{
		return SavedDir() + "Log/";
	}
	std::string paths::RenderGraphDir()
	{
		return SavedDir() + "RenderGraph/";
	}
	std::string paths::ShaderCacheDir()
	{
		return SavedDir() + "ShaderCache/";
	}
	std::string paths::IniDir()
	{
		return SavedDir() + "Ini/";
	}

}

