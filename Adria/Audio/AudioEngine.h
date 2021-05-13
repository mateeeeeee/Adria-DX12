#pragma once
#include <optional>
#include <memory>
#include <string>

namespace adria
{

	using CHANNEL_HANDLE = int;
	using CHANNEL_GROUP_HANDLE = int;
	using SOUND_HANDLE = int;

	struct Vector3
	{
		float x, y, z;
	};

	struct AudioDescription
	{
		std::string path;
		bool is_3d = false;
		bool is_loop = false;
		bool is_stream = false;
	};

	struct AudioPlayArguments
	{
		SOUND_HANDLE handle;
		float volume;
		bool paused = false;
		std::optional<Vector3> source_position;
		std::optional<std::pair<float, float>> min_max_distance;

		AudioPlayArguments(SOUND_HANDLE handle, float volume) : handle{ handle }, volume{ volume } {}

		void SetPaused(bool _paused)
		{
			paused = _paused;
		}

		void SetMinMaxDistances(float min, float max)
		{
			min_max_distance = { min,max };
		}

		void SetSourcePosition(Vector3 const& position)
		{
			source_position = position;
		}

	};


	class AudioEngine
	{
	public:

		explicit AudioEngine(int max_channels);

		AudioEngine(AudioEngine const&) = delete;
		AudioEngine(AudioEngine&&) = delete;

		AudioEngine& operator=(AudioEngine const&) = delete;
		AudioEngine& operator=(AudioEngine&&) = delete;

		~AudioEngine();

		void Update(float dt);

		[[nodiscard]] SOUND_HANDLE LoadSound(AudioDescription const&);

		void UnloadSound(SOUND_HANDLE);

		[[nodiscard]] CHANNEL_HANDLE PlaySound(AudioPlayArguments const&);

		void SetChannelVolume(CHANNEL_HANDLE, float);

		void SetListener(Vector3 const& camera_pos, Vector3 const& camera_look, Vector3 camera_up);

		void SetChannelPosition(CHANNEL_HANDLE, Vector3 const& position);

		bool IsPlaying(CHANNEL_HANDLE) const;

		void ToggleChannelPause(CHANNEL_HANDLE);

		void StopChannel(CHANNEL_HANDLE);

		void FadeChannel(CHANNEL_HANDLE, float fade_time);

		//global

		void SetVolume(float);

		void StopAllChannels();

		void TogglePause();

		//channel groups

		[[nodiscard]] CHANNEL_GROUP_HANDLE CreateChannelGroup();

		void AddToChannelGroup(CHANNEL_GROUP_HANDLE, CHANNEL_HANDLE);

		void StopGroup(CHANNEL_GROUP_HANDLE);

		void SetGroupVolume(CHANNEL_GROUP_HANDLE, float);

		void ToggleGroupPause(CHANNEL_GROUP_HANDLE);

	private:
		class Impl;
		std::unique_ptr<Impl> pImpl;
	};

}