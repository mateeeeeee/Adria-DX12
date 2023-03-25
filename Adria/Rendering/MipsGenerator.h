#pragma once
#include <memory>
#include <vector>
#include "../Core/CoreTypes.h"

namespace adria
{
	class GfxDevice;
	class GfxCommandList;
	class GfxTexture;
	class GfxDescriptorAllocator;

	class MipsGenerator
	{
	public:

		MipsGenerator(GfxDevice* gfx, uint32 max_textures);
		~MipsGenerator();
		void Add(GfxTexture* texture);
		void Generate(GfxCommandList* command_list);

	private:
		GfxDevice* gfx;
		std::unique_ptr<GfxDescriptorAllocator> descriptor_allocator;
		std::vector<GfxTexture*> resources;

	private:
		void CreateHeap(uint32 max_textures); //approximate number of descriptors as : ~ max_textures * 2 * 10 (avg mip levels)

	};
}