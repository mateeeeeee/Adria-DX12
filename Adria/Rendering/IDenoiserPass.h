#pragma once

namespace adria
{
	enum DenoiserType : Int
	{
		DenoiserType_None,
		DenoiserType_OIDN,
		DenoiserType_SVGF
	};

	class GfxDevice;
	class RenderGraph;
	class IDenoiserPass
	{
	public:
		virtual ~IDenoiserPass() = default;
		virtual Bool IsSupported() const = 0;
		virtual void AddPass(RenderGraph& rendergraph) = 0;
		virtual void Reset() = 0;
		virtual DenoiserType GetType() const = 0;
	};

	class DummyDenoiserPass : public IDenoiserPass
	{
	public:
		DummyDenoiserPass() {}
		virtual Bool IsSupported() const override { return true; }
		virtual void AddPass(RenderGraph& rendergraph) override {}
		virtual void Reset() override {}
		virtual DenoiserType GetType() const override { return DenoiserType_None; }
	};

	IDenoiserPass* CreateDenoiser(GfxDevice* gfx, DenoiserType type);
}