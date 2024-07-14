#pragma once


namespace adria
{
	struct PickingData
	{
		Vector4 position;
		Vector4 normal;
	};

	class GfxDevice;
	class GfxBuffer;
	class GfxComputePipelineState;
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
		uint32 width, height;
		std::vector<std::unique_ptr<GfxBuffer>> read_picking_buffers;
		std::unique_ptr<GfxComputePipelineState> picking_pso;

	private:
		void CreatePSO();
		void CreatePickingBuffers();
	};
}