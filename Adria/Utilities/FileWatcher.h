#pragma once
#include <filesystem>
#include "Utilities/Delegate.h"

namespace adria
{
	enum class FileStatus : uint8
	{
		Created,
		Modified,
		Deleted
	};

	DECLARE_EVENT(FileModifiedEvent, FileWatcher, std::string const&)

	class FileWatcher
	{
	public:
		FileWatcher() = default;
		~FileWatcher()
		{
			file_modified_event.RemoveAll();
		}

		void AddPathToWatch(std::string const& path, bool recursive = true)
		{
			if (recursive)
			{
				for (auto& path : std::filesystem::recursive_directory_iterator(path))
				{
					if (path.is_regular_file()) files_map[path.path().string()] = std::filesystem::last_write_time(path);
				}
			}
			else
			{
				for (auto& path : std::filesystem::directory_iterator(path))
				{
					if (path.is_regular_file()) files_map[path.path().string()] = std::filesystem::last_write_time(path);
				}
			}
		}
		void CheckWatchedFiles()
		{
			for (auto const& [file, ft] : files_map)
			{
				auto current_file_last_write_time = std::filesystem::last_write_time(file);
				if (files_map[file] != current_file_last_write_time)
				{
					files_map[file] = current_file_last_write_time;
					file_modified_event.Broadcast(file);
				}
			}
		}

		FileModifiedEvent& GetFileModifiedEvent() { return file_modified_event; }
	private:
		std::vector<std::string> paths_to_watch;
		std::unordered_map<std::string, std::filesystem::file_time_type> files_map;
		FileModifiedEvent file_modified_event;
	};
}