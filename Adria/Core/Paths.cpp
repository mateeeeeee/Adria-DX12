#include "Paths.h"

namespace adria
{

	std::string const paths::MainDir = "";

	std::string const paths::ConfigDir = MainDir;

	std::string const paths::ResourcesDir = MainDir + "Resources/";

	std::string const paths::SavedDir = MainDir + "Saved/";

	std::string const paths::ToolsDir = MainDir + "Tools/";

	std::string const paths::FontsDir = ResourcesDir + "Fonts/";
	
	std::string const paths::IconsDir = ResourcesDir + "Icons/";

	std::string const paths::ShaderDir = ResourcesDir + "Shaders/";

	std::string const paths::TexturesDir = ResourcesDir + "Textures/";

	std::string const paths::ScreenshotsDir = SavedDir + "Screenshots/";

	std::string const paths::LogDir = SavedDir + "Log/";
	
	std::string const paths::RenderGraphDir = SavedDir + "RenderGraph/";

	std::string const paths::ShaderCacheDir = SavedDir + "ShaderCache/";

	std::string const paths::ShaderPDBDir = SavedDir + "ShaderPDB/";

	std::string const paths::IniDir = SavedDir + "Ini/";

	std::string const paths::ScenesDir = SavedDir + "Scenes/";

	std::string const paths::AftermathDir = SavedDir + "Aftermath/";

	std::string const paths::NsightPerfReportDir = SavedDir + "NsightPerfReport/";

	std::string const paths::PixCapturesDir = SavedDir + "PixCaptures/";
}

