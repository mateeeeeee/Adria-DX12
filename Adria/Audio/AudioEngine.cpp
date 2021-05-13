#include "AudioEngine.h"
#include <unordered_map>
#include <algorithm>
#include <cassert>

#include <fmod.hpp>
#include <fmod_errors.h>

#ifdef _WIN32
#include <combaseapi.h>
#undef PlaySound
#endif

#ifdef _DEBUG
#pragma comment(lib, "fmodL_vc.lib") 
#else
#pragma comment(lib, "fmod_vc.lib") 
#endif

namespace adria
{


	void FMOD_ASSERT(FMOD_RESULT result)
	{
		assert(result == FMOD_OK && FMOD_ErrorString(result));
	}

	FMOD_VECTOR Vector3ToFmodVector(Vector3 const& v)
	{
		return FMOD_VECTOR{ v.x,v.y,v.z };
	}

	template<typename FMODType>
	struct FMODDeleter
	{
		void operator()(FMODType* fmod_object)
		{
			fmod_object->release();
			fmod_object = nullptr;
		}
	};

	template<typename FMODType>
	using fmod_ptr = std::unique_ptr<FMODType, FMODDeleter<FMODType>>;

	class Channel
	{
		enum class State { PLAYING, FADING, STOPPED };

	public:
		Channel() {}

		Channel(FMOD::Channel* channel)
			: _channel(channel), current_state(State::PLAYING)
		{

		}

		~Channel() = default;

		void Update(float delta_time)
		{
			switch (current_state)
			{
			case State::PLAYING:
				if (fade_requested)
				{
					fade_requested = false;
					current_state = State::FADING;
				}
				else if (stop_requested)
				{
					stop_requested = false;
					current_state = State::STOPPED;
				}
				break;
			case State::FADING:
				if (stop_requested)
				{
					stop_requested = false;
					current_state = State::STOPPED;
				}
				else
				{
					bool paused;
					_channel->getPaused(&paused);

					if (!paused)
					{
						fade_time_left -= delta_time;
						float volume = (fade_time_left / fade_time) * fade_start_volume;

						_channel->setVolume(volume);

						if (fade_time_left <= 0.0f) current_state = State::STOPPED;
					}
				}
				break;
			case State::STOPPED:
			default:
				break;
			}
		}

		void RequestStop()
		{
			stop_requested = true;
		}

		void RequestFade(float _fade_time)
		{
			fade_requested = true;
			fade_time = _fade_time;
			fade_time_left = fade_time;

			_channel->getVolume(&fade_start_volume);
		}

		bool IsStopped() const
		{
			return current_state == State::STOPPED;
		}

		void SetVolume(float _volume)
		{
			if (current_state != State::FADING) _channel->setVolume(_volume);
		}

		void TogglePause()
		{
			bool paused;
			_channel->getPaused(&paused);
			_channel->setPaused(!paused);
		}

		void SetChannelGroup(FMOD::ChannelGroup* group)
		{
			_channel->setChannelGroup(group);
		}

		void SetPosition(Vector3 const& position)
		{
			FMOD_VECTOR pos = Vector3ToFmodVector(position);
			_channel->set3DAttributes(&pos, nullptr);
		}

	private:

		FMOD::Channel* _channel = nullptr;
		State current_state = State::STOPPED;
		bool  stop_requested = false;
		bool  fade_requested = false;
		float fade_time = 0.0f;
		float fade_time_left = 0.0f;
		float fade_start_volume = 0.0f;
	};

	using SoundMap = std::unordered_map<SOUND_HANDLE, fmod_ptr<FMOD::Sound>>;
	using ChannelGroupMap = std::unordered_map<CHANNEL_GROUP_HANDLE, fmod_ptr<FMOD::ChannelGroup>>;
	using ChannelMap = std::unordered_map<CHANNEL_HANDLE, Channel>;

	class AudioEngine::Impl
	{
		friend AudioEngine;

	private:
		Impl(int max_channels) : _sounds{}, _channels{}, _channel_groups{},
			_nextchannel_handle{ 0 }, _nextsound_handle{ 0 }, _nextchannel_group_handle{ 0 }
		{
			FMOD::System* system = nullptr;
			FMOD_ASSERT(FMOD::System_Create(&system));
			_system.reset(system);
			FMOD_ASSERT(_system->init(max_channels, FMOD_INIT_NORMAL, 0));
		}

		void Update(float delta_time)
		{

			for (auto it = std::begin(_channels); it != std::end(_channels); )
			{
				it->second.Update(delta_time);
				if (it->second.IsStopped()) it = _channels.erase(it);
				else ++it;
			}

			_system->update();
		}


	private:
		fmod_ptr<FMOD::System> _system;
		std::unordered_map<std::string, SOUND_HANDLE> _sound_names;

		SoundMap _sounds;
		ChannelMap _channels;
		ChannelGroupMap _channel_groups;

		CHANNEL_HANDLE _nextchannel_handle;
		SOUND_HANDLE _nextsound_handle;
		CHANNEL_GROUP_HANDLE _nextchannel_group_handle;
	};

	AudioEngine::~AudioEngine()
	{
#ifdef _WIN32
		CoUninitialize();
#endif
	}

	AudioEngine::AudioEngine(int max_channels)
	{
#ifdef _WIN32
		HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		if (FAILED(hr)) assert(false && "COM initialization failed!");
#endif
		pImpl.reset(new Impl{ max_channels });
	}

	void AudioEngine::Update(float delta_time)
	{
		pImpl->Update(delta_time);
	}

	SOUND_HANDLE AudioEngine::LoadSound(AudioDescription const& audio_desc)
	{
		auto sound_names_it = pImpl->_sound_names.find(audio_desc.path);

		SOUND_HANDLE sound_handle = (sound_names_it == pImpl->_sound_names.end()) ? pImpl->_nextsound_handle++ : sound_names_it->second;

		if (auto sound_it = pImpl->_sounds.find(sound_handle); sound_it == pImpl->_sounds.end())
		{
			FMOD_MODE eMode = FMOD_DEFAULT;

			eMode |= audio_desc.is_3d ? FMOD_3D : FMOD_2D;
			eMode |= audio_desc.is_loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
			eMode |= audio_desc.is_stream ? FMOD_CREATESTREAM : FMOD_CREATECOMPRESSEDSAMPLE;

			FMOD::Sound* sound = nullptr;

			FMOD_ASSERT(pImpl->_system->createSound(audio_desc.path.c_str(), eMode, nullptr, &sound));

			if (sound) pImpl->_sounds[sound_handle] = fmod_ptr<FMOD::Sound>(sound);
		}

		return sound_handle;
	}

	void AudioEngine::UnloadSound(SOUND_HANDLE sound_handle)
	{
		if (auto sound_it = pImpl->_sounds.find(sound_handle); sound_it != pImpl->_sounds.end())
			pImpl->_sounds.erase(sound_it);
	}

	CHANNEL_HANDLE AudioEngine::PlaySound(AudioPlayArguments const& play_args)
	{
		CHANNEL_HANDLE channel_handle = pImpl->_nextchannel_handle++; //should be changed to something better

		if (auto sound_it = pImpl->_sounds.find(play_args.handle); sound_it != pImpl->_sounds.end())
		{
			FMOD::Channel* channel = nullptr;

			pImpl->_system->playSound(sound_it->second.get(), nullptr, true, &channel);

			if (channel)
			{
				channel->setVolume(play_args.volume);

				if (play_args.source_position.has_value())
				{
					FMOD_VECTOR pos = Vector3ToFmodVector(play_args.source_position.value());
					channel->set3DAttributes(&pos, nullptr);
				}

				if (play_args.min_max_distance.has_value())
				{
					auto const& min_max_pair = play_args.min_max_distance.value();
					channel->set3DMinMaxDistance(min_max_pair.first, min_max_pair.second);
				}

				channel->setPaused(play_args.paused);

				Channel ch(channel);

				pImpl->_channels[channel_handle] = ch;
			}
		}

		return channel_handle;
	}

	void AudioEngine::SetChannelVolume(CHANNEL_HANDLE channel, float v)
	{
		if (auto channel_it = pImpl->_channels.find(channel); channel_it != pImpl->_channels.end())
		{
			channel_it->second.SetVolume(v);
		}
	}

	void AudioEngine::SetChannelPosition(CHANNEL_HANDLE channel, Vector3 const& position)
	{
		if (auto channel_it = pImpl->_channels.find(channel); channel_it != pImpl->_channels.end())
		{
			channel_it->second.SetPosition(position);
		}
	}

	void AudioEngine::StopChannel(CHANNEL_HANDLE channel)
	{
		if (auto channel_it = pImpl->_channels.find(channel); channel_it != pImpl->_channels.end())
		{
			channel_it->second.RequestStop();
		}

	}

	void AudioEngine::FadeChannel(CHANNEL_HANDLE channel, float fade_time)
	{

		if (fade_time <= 0.0f) return StopChannel(channel);

		if (auto channel_it = pImpl->_channels.find(channel); channel_it != pImpl->_channels.end())
		{
			channel_it->second.RequestFade(fade_time);
		}
	}

	void AudioEngine::ToggleChannelPause(CHANNEL_HANDLE channel)
	{
		if (auto channel_it = pImpl->_channels.find(channel); channel_it != pImpl->_channels.end())
		{
			channel_it->second.TogglePause();
		}

	}

	bool AudioEngine::IsPlaying(CHANNEL_HANDLE channel) const
	{
		bool is_playing = false;

		if (auto channel_it = pImpl->_channels.find(channel); channel_it != pImpl->_channels.end())
		{
			return !channel_it->second.IsStopped();
		}

		return is_playing;

	}

	void AudioEngine::StopAllChannels()
	{
		FMOD::ChannelGroup* master_group;
		pImpl->_system->getMasterChannelGroup(&master_group);

		if (master_group) master_group->stop();
	}

	void AudioEngine::TogglePause()
	{
		FMOD::ChannelGroup* master_group;
		pImpl->_system->getMasterChannelGroup(&master_group);

		if (master_group)
		{
			bool is_paused;
			master_group->getPaused(&is_paused);
			master_group->setPaused(!is_paused);
		}
	}

	void AudioEngine::SetVolume(float volume)
	{
		FMOD::ChannelGroup* master_group;
		pImpl->_system->getMasterChannelGroup(&master_group);

		if (master_group) master_group->setVolume(volume);
	}

	void AudioEngine::SetListener(Vector3 const& camera_pos, Vector3 const& camera_look, Vector3 camera_up)
	{
		FMOD_VECTOR pos = Vector3ToFmodVector(camera_pos);
		FMOD_VECTOR look = Vector3ToFmodVector(camera_look);
		FMOD_VECTOR up = Vector3ToFmodVector(camera_up);

		pImpl->_system->set3DListenerAttributes(0, &pos, nullptr, &look, &up);
	}

	CHANNEL_GROUP_HANDLE AudioEngine::CreateChannelGroup()
	{
		CHANNEL_GROUP_HANDLE handle = pImpl->_nextchannel_group_handle++;

		FMOD::ChannelGroup* group;

		pImpl->_system->createChannelGroup(nullptr, &group);

		if (group) pImpl->_channel_groups[handle] = fmod_ptr<FMOD::ChannelGroup>(group);

		return handle;
	}

	void AudioEngine::AddToChannelGroup(CHANNEL_GROUP_HANDLE group_handle, CHANNEL_HANDLE channel_handle)
	{
		if (auto group_it = pImpl->_channel_groups.find(group_handle); group_it != pImpl->_channel_groups.end())
		{
			if (auto channel_it = pImpl->_channels.find(channel_handle); channel_it != pImpl->_channels.end())
			{
				channel_it->second.SetChannelGroup(group_it->second.get());
			}
		}
	}

	void AudioEngine::StopGroup(CHANNEL_GROUP_HANDLE group_handle)
	{
		if (auto group_it = pImpl->_channel_groups.find(group_handle); group_it != pImpl->_channel_groups.end())
			group_it->second->stop();
	}

	void AudioEngine::SetGroupVolume(CHANNEL_GROUP_HANDLE group_handle, float volume)
	{
		if (auto group_it = pImpl->_channel_groups.find(group_handle); group_it != pImpl->_channel_groups.end())
			group_it->second->setVolume(volume);
	}

	void AudioEngine::ToggleGroupPause(CHANNEL_GROUP_HANDLE group_handle)
	{
		if (auto group_it = pImpl->_channel_groups.find(group_handle); group_it != pImpl->_channel_groups.end())
		{
			bool paused;
			group_it->second->getPaused(&paused);
			group_it->second->setPaused(!paused);
		}

	}

}