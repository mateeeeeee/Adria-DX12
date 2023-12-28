#pragma once
#include <string>

namespace adria
{
	std::string GetParentPath(std::string_view complete_path);
	std::string GetFilename(std::string_view complete_path);
	std::string GetFilenameWithoutExtension(std::string_view complete_path);
	bool FileExists(std::string_view file_path);
	std::string GetExtension(std::string_view path);
	long long GetFileLastWriteTime(std::string_view file_path);

	void NormalizePathInline(std::string& file_path);
	std::string NormalizePath(std::string_view file_path);
	bool ResolveRelativePaths(std::string& path);
}