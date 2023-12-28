#include <filesystem>
#include "FilesUtil.h"

namespace fs = std::filesystem;

namespace adria
{
	std::string GetParentPath(std::string_view complete_path)
	{
		fs::path p(complete_path);
		return p.parent_path().string();
	}
	std::string GetFilename(std::string_view complete_path)
	{
		fs::path p(complete_path);
		return p.filename().string();
	}
	std::string GetFilenameWithoutExtension(std::string_view complete_path)
	{
		fs::path p(complete_path);
		return p.filename().replace_extension().string();
	}
	bool FileExists(std::string_view file_path)
	{
		fs::path p(file_path);
		return fs::exists(p);
	}
	std::string GetExtension(std::string_view path)
	{
		fs::path p(path);
		return p.extension().string();
	}
	long long GetFileLastWriteTime(std::string_view file_path)
	{
		fs::path p(file_path);
		return fs::last_write_time(p).time_since_epoch().count();
	}

	void NormalizePathInline(std::string& file_path)
	{
		for (char& c : file_path)
		{
			if (c == '\\')
			{
				c = '/';
			}
		}
		if (file_path.find("./") == 0)
		{
			file_path = std::string(file_path.begin() + 2, file_path.end());
		}
	}
	std::string NormalizePath(std::string_view file_path)
	{
		std::string output = std::string(file_path.begin(), file_path.end());
		NormalizePathInline(output);
		return output;
	}
	bool ResolveRelativePaths(std::string& path)
	{
		while (true)
		{
			size_t index = path.rfind("../");
			if (index == std::string::npos)
				break;
			size_t idx0 = path.rfind('/', index);
			if (idx0 == std::string::npos)
				return false;
			idx0 = path.rfind('/', idx0 - 1);
			if (idx0 != std::string::npos)
				path = path.substr(0, idx0 + 1) + path.substr(index + 3);
		}
		return true;
	}

}