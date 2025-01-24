#pragma once
#include <memory>
#include <d3d12.h>
#include "GfxMacros.h"
#include "Utilities/Ref.h"

namespace adria
{
	enum class IndirectCommandType : Uint8
	{
		Draw,
		DrawIndexed,
		Dispatch,
		DispatchMesh
	};
	
	class GfxDevice;
	class IndirectCommandSignature
	{
	public:
		IndirectCommandSignature(GfxDevice* device, IndirectCommandType cmd_type);
		operator ID3D12CommandSignature* () const
		{
			return cmd_signature.Get();
		}
	private:
		Ref<ID3D12CommandSignature> cmd_signature;
	};

	class DrawIndirectSignature : public IndirectCommandSignature
	{
	public:
		explicit DrawIndirectSignature(GfxDevice* gfx) : IndirectCommandSignature(gfx, IndirectCommandType::Draw) {}
	};

	class DrawIndexedIndirectSignature : public IndirectCommandSignature
	{
	public:
		explicit DrawIndexedIndirectSignature(GfxDevice* gfx) : IndirectCommandSignature(gfx, IndirectCommandType::DrawIndexed) {}
	};
	
	class DispatchIndirectSignature : public IndirectCommandSignature
	{
	public:
		explicit DispatchIndirectSignature(GfxDevice* gfx) : IndirectCommandSignature(gfx, IndirectCommandType::Dispatch) {}
	};
	
	class DispatchMeshIndirectSignature : public IndirectCommandSignature
	{
	public:
		explicit DispatchMeshIndirectSignature(GfxDevice* gfx) : IndirectCommandSignature(gfx, IndirectCommandType::DispatchMesh) {}
	};
}