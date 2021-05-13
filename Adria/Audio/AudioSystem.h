#pragma once
#include "AudioEngine.h"

namespace adria
{
	//global audio engine

	namespace AudioSystem
	{
		void Initialize(int max_channels);

		void Destroy();

		void Update(float dt);

		[[nodiscard]] SOUND_HANDLE LoadSound(AudioDescription const&);

		void UnloadSound(SOUND_HANDLE);

		[[nodiscard]] CHANNEL_HANDLE PlaySound(AudioPlayArguments const&);

		void SetChannelVolume(CHANNEL_HANDLE, float);

		void SetListener(Vector3 const& camera_pos, Vector3 const& camera_look, Vector3 camera_up);

		void SetChannelPosition(CHANNEL_HANDLE, Vector3 const& position);

		bool IsPlaying(CHANNEL_HANDLE);

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
	}

}