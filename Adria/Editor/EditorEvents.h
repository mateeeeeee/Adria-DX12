#pragma once
#include "../Events/Delegate.h"

namespace adria
{


	DECLARE_EVENT(ParticleEmitterAddedEvent, Editor, size_t)
	DECLARE_EVENT(ParticleEmitterRemovedEvent, Editor, size_t)

	struct EditorEvents
	{
		ParticleEmitterAddedEvent particle_emitter_added;
		ParticleEmitterAddedEvent particle_emitter_removed;
	};
}