#pragma once
#include <filesystem>

namespace fs = std::filesystem;


namespace adria
{
	inline std::string GetPath(std::string const& complete_path)
	{
		fs::path p(complete_path);

		return p.parent_path().string();
	}

	inline std::string GetFilename(std::string const& complete_path)
	{
		fs::path p(complete_path);

		return p.filename().string();
	}

	inline std::string GetExtension(std::string const& path)
	{
		fs::path p(path);
		return p.extension().string();
	}
}