#pragma once
#include <string>
#include <array>
#include <d3d12.h>
#include <wrl.h>
#include <variant>
#include <unordered_map>
#include <memory>
#include "MipsGenerator.h"
#include "ResourceBarrierBatch.h"

namespace adria
{
	class GraphicsDevice;
	class DescriptorHeap;
	
	using TEXTURE_HANDLE = size_t;
	inline constexpr TEXTURE_HANDLE INVALID_TEXTURE_HANDLE = TEXTURE_HANDLE(-1);

	class TextureManager
	{
		friend class Renderer;

	public:
		TextureManager(GraphicsDevice* gfx, UINT max_textures);

		[[nodiscard]] TEXTURE_HANDLE LoadTexture(std::wstring const& name);

		[[nodiscard]] TEXTURE_HANDLE LoadCubemap(std::wstring const& name);

		[[nodiscard]] TEXTURE_HANDLE LoadCubemap(std::array<std::wstring, 6> const& cubemap_textures);

		[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE CpuDescriptorHandle(TEXTURE_HANDLE tex_handle);

		//remove this, only lens flare textures use this
		void TransitionTexture(TEXTURE_HANDLE handle, ResourceBarrierBatch& barrier,
			D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after);

		ID3D12Resource* Resource(TEXTURE_HANDLE handle) const
		{
			if (handle == INVALID_TEXTURE_HANDLE) return nullptr;
			else if (auto it = texture_map.find(handle); it != texture_map.end()) return it->second.Get();
			else return nullptr;
		}

		void SetMipMaps(bool);

		void GenerateAllMips()
		{
			mips_generator->Generate(gfx->GetDefaultCommandList());
		}

	private:
		GraphicsDevice* gfx;
		std::unique_ptr<DescriptorHeap> texture_srv_heap = nullptr;
		std::unique_ptr<MipsGenerator> mips_generator = nullptr;

		TEXTURE_HANDLE handle = 0;
		std::unordered_map<TEXTURE_HANDLE, Microsoft::WRL::ComPtr<ID3D12Resource>> texture_map{};
		std::unordered_map<std::variant<std::wstring, std::string>, TEXTURE_HANDLE> loaded_textures{};
		bool mipmaps = true;

		Microsoft::WRL::ComPtr<ID3D12RootSignature> equirect_root_signature;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> equirect_pso;
		
	private:

		TEXTURE_HANDLE LoadDDSTexture(std::wstring const& texture_path);

		TEXTURE_HANDLE LoadWICTexture(std::wstring const& texture_path);

		TEXTURE_HANDLE LoadTexture_HDR_TGA_PIC(std::string const& texture_path);
	};
}