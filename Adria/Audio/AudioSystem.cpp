#include "AudioSystem.h"
#include <cassert>

namespace adria
{

	namespace
	{
		std::unique_ptr<AudioEngine> global_audio_engine = nullptr;
	}

	void AudioSystem::Initialize(int max_channels)
	{
		global_audio_engine = std::make_unique<AudioEngine>(max_channels);
	}

	void AudioSystem::Destroy()
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine.reset(nullptr);
	}

	void AudioSystem::Update(float dt)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->Update(dt);
	}

	SOUND_HANDLE AudioSystem::LoadSound(AudioDescription const& ad)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		return global_audio_engine->LoadSound(ad);
	}

	void AudioSystem::UnloadSound(SOUND_HANDLE handle)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->UnloadSound(handle);
	}

	CHANNEL_HANDLE AudioSystem::PlaySound(AudioPlayArguments const& args)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		return global_audio_engine->PlaySound(args);
	}

	void AudioSystem::SetChannelVolume(CHANNEL_HANDLE handle, float volume)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->SetChannelVolume(handle, volume);
	}

	void AudioSystem::SetListener(Vector3 const& camera_pos, Vector3 const& camera_look, Vector3 camera_up)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->SetListener(camera_pos, camera_look, camera_up);
	}

	void AudioSystem::SetChannelPosition(CHANNEL_HANDLE handle, Vector3 const& position)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->SetChannelPosition(handle, position);
	}

	bool AudioSystem::IsPlaying(CHANNEL_HANDLE handle)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		return global_audio_engine->IsPlaying(handle);
	}

	void AudioSystem::ToggleChannelPause(CHANNEL_HANDLE handle)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->ToggleChannelPause(handle);
	}

	void AudioSystem::StopChannel(CHANNEL_HANDLE handle)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->StopChannel(handle);
	}

	void AudioSystem::FadeChannel(CHANNEL_HANDLE handle, float fade_time)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->FadeChannel(handle, fade_time);
	}

	void AudioSystem::SetVolume(float volume)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->SetVolume(volume);
	}

	void AudioSystem::StopAllChannels()
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->StopAllChannels();
	}

	void AudioSystem::TogglePause()
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->TogglePause();
	}

	CHANNEL_GROUP_HANDLE AudioSystem::CreateChannelGroup()
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		return global_audio_engine->CreateChannelGroup();
	}

	void AudioSystem::AddToChannelGroup(CHANNEL_GROUP_HANDLE group_handle, CHANNEL_HANDLE handle)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->AddToChannelGroup(group_handle, handle);
	}

	void AudioSystem::StopGroup(CHANNEL_GROUP_HANDLE group_handle)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->StopGroup(group_handle);
	}

	void AudioSystem::SetGroupVolume(CHANNEL_GROUP_HANDLE group_handle, float volume)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->SetGroupVolume(group_handle, volume);
	}

	void AudioSystem::ToggleGroupPause(CHANNEL_GROUP_HANDLE group_handle)
	{
		assert(global_audio_engine && "Audio Not Initialized!");
		global_audio_engine->ToggleGroupPause(group_handle);
	}


}