#pragma once
#include "../Graphics/Buffer.h"
#include "../Graphics/Texture.h"

namespace adria
{
	class RenderGraphResourcePool
	{
		struct PooledTexture
		{
			std::unique_ptr<Texture> texture;
			uint64 last_used_frame;
		};

		struct PooledBuffer
		{
			std::unique_ptr<Buffer> buffer;
			uint64 last_used_frame;
		};

	public:
		explicit RenderGraphResourcePool(GraphicsDevice* device) : device(device) {}

		void Tick()
		{
			for (size_t i = 0; i < texture_pool.size();)
			{
				PooledTexture& resource = texture_pool[i].first;
				bool active = texture_pool[i].second;
				if (!active && resource.last_used_frame + 3 < frame_index)
				{
					std::swap(texture_pool[i], texture_pool.back());
					texture_pool.pop_back();
				}
				else ++i;
			}
			++frame_index;
		}

		Texture* AllocateTexture(TextureDesc const& desc)
		{
			for (auto& [pool_texture, active] : texture_pool)
			{
				if (!active && pool_texture.texture->GetDesc().IsCompatible(desc))
				{
					pool_texture.last_used_frame = frame_index;
					active = true;
					return pool_texture.texture.get();
				}
			}
			auto& texture = texture_pool.emplace_back(std::pair{ PooledTexture{ std::make_unique<Texture>(device, desc), frame_index}, true }).first.texture;
			return texture.get();
		}
		void ReleaseTexture(Texture* texture)
		{
			for (auto& [pooled_texture, active] : texture_pool)
			{
				auto& texture_ptr = pooled_texture.texture;
				if (active && texture_ptr.get() == texture)
				{
					active = false;
				}
			}
		}

		Buffer* AllocateBuffer(BufferDesc const& desc)
		{
			for (auto& [pool_buffer, active] : buffer_pool)
			{
				if (!active && pool_buffer.buffer->GetDesc() == desc)
				{
					pool_buffer.last_used_frame = frame_index;
					active = true;
					return pool_buffer.buffer.get();
				}
			}
			auto& buffer = buffer_pool.emplace_back(std::pair{ PooledBuffer{ std::make_unique<Buffer>(device, desc), frame_index}, true }).first.buffer;
			return buffer.get();
		}
		void ReleaseBuffer(Buffer* buffer)
		{
			for (auto& [pooled_buffer, active] : buffer_pool)
			{
				auto& buffer_ptr = pooled_buffer.buffer;
				if (active && buffer_ptr.get() == buffer)
				{
					active = false;
				}
			}
		}

		GraphicsDevice* GetDevice() const { return device; }
	private:
		GraphicsDevice* device = nullptr;
		uint64 frame_index = 0;
		std::vector<std::pair<PooledTexture, bool>> texture_pool;
		std::vector<std::pair<PooledBuffer, bool>>  buffer_pool;
	};
	using RGResourcePool = RenderGraphResourcePool;

}