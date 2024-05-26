#include "GfxStateObject.h"
#include "GfxDevice.h"

namespace adria
{
	GfxStateObject* GfxStateObjectBuilder::CreateStateObject(GfxDevice* gfx, GfxStateObjectType type)
	{
		D3D12_STATE_OBJECT_TYPE d3d12_type;
		switch (type)
		{
		case GfxStateObjectType::RayTracingPipeline: d3d12_type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE; break;
		case GfxStateObjectType::Collection:		 d3d12_type = D3D12_STATE_OBJECT_TYPE_COLLECTION; break;
		case GfxStateObjectType::Executable:		 d3d12_type = D3D12_STATE_OBJECT_TYPE_EXECUTABLE; break;
		default: d3d12_type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
		}

		auto BuildDescription = [this](D3D12_STATE_OBJECT_TYPE type, D3D12_STATE_OBJECT_DESC& desc)
		{
			desc.Type = type;
			desc.NumSubobjects = static_cast<uint32>(num_subobjects);
			desc.pSubobjects = num_subobjects ? subobjects.data() : nullptr;
		};
		D3D12_STATE_OBJECT_DESC desc{};
		BuildDescription(d3d12_type, desc);
		ID3D12StateObject* state_obj = nullptr;
		HRESULT hr = gfx->GetDevice()->CreateStateObject(&desc, IID_PPV_ARGS(&state_obj));
		GFX_CHECK_HR(hr);
		return new GfxStateObject(state_obj);
	}

}
