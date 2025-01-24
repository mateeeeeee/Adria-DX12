#include "GfxCommandSignature.h"
#include "GfxDevice.h"

namespace adria
{
	static D3D12_INDIRECT_ARGUMENT_TYPE GetArgumentType(IndirectCommandType cmd_type)
	{
		switch (cmd_type)
		{
		case IndirectCommandType::Draw:			return D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
		case IndirectCommandType::DrawIndexed:	return D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
		case IndirectCommandType::Dispatch:		return D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
		case IndirectCommandType::DispatchMesh:	return D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
		}
		ADRIA_ASSERT(false);
		return D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
	}
	static Uint32 GetArgumentStride(IndirectCommandType cmd_type)
	{
		switch (cmd_type)
		{
		case IndirectCommandType::Draw:			return sizeof(D3D12_DRAW_ARGUMENTS);
		case IndirectCommandType::DrawIndexed:	return sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
		case IndirectCommandType::Dispatch:		return sizeof(D3D12_DISPATCH_ARGUMENTS);
		case IndirectCommandType::DispatchMesh:	return sizeof(D3D12_DISPATCH_MESH_ARGUMENTS);
		}
		ADRIA_ASSERT(false);
		return 0;
	}

	IndirectCommandSignature::IndirectCommandSignature(GfxDevice* gfx, IndirectCommandType cmd_type)
	{
		D3D12_COMMAND_SIGNATURE_DESC desc{};
		D3D12_INDIRECT_ARGUMENT_DESC argument_desc{};
		desc.NumArgumentDescs = 1;
		desc.pArgumentDescs = &argument_desc;
		desc.ByteStride = GetArgumentStride(cmd_type);
		argument_desc.Type = GetArgumentType(cmd_type);
		GFX_CHECK_HR(gfx->GetDevice()->CreateCommandSignature(&desc, nullptr, IID_PPV_ARGS(cmd_signature.GetAddressOf())));
	}
}

