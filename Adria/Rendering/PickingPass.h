#pragma once
#include <memory>
#include <vector>
#include <DirectXMath.h>
#include "Graphics/GfxBuffer.h"


namespace adria
{
	struct PickingData
	{
		DirectX::XMFLOAT4 position;
		DirectX::XMFLOAT4 normal;
	};

	class GfxDevice;
	class RenderGraph;

	class PickingPass
	{
	public:
		
		PickingPass(GfxDevice* gfx, uint32 width, uint32 height);

		void OnResize(uint32 w, uint32 h);

		void AddPass(RenderGraph& rg);

		PickingData GetPickingData() const;

	private:
		GfxDevice* gfx;
		std::vector<std::unique_ptr<GfxBuffer>> read_picking_buffers;
		uint32 width, height;
	};
}