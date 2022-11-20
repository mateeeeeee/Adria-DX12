#pragma once
#include "GraphicsDeviceDX12.h"

namespace adria
{

	class MipsGenerator
	{
	public:

		MipsGenerator(GraphicsDevice* gfx, UINT max_textures);
		void Add(ID3D12Resource* texture);
		void Generate(ID3D12GraphicsCommandList* command_list);

	private:
		GraphicsDevice* gfx;
		std::unique_ptr<LinearOnlineDescriptorAllocator> descriptor_allocator;
		std::vector<ID3D12Resource*> resources;
	private:

		void CreateHeap(UINT max_textures); //approximate number of descriptors as : ~ max_textures * 2 * 10 (avg mip levels)

	};
}