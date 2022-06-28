#pragma once
#include <d3d12.h>
#include "Enums.h"
#include "RootSignatureCache.h"
#include "ShaderManager.h"
#include "../Core/Macros.h"

//struct ID3D12PipelineState;

namespace adria
{
	enum class ERasterizerState
	{
		Default,
		NoCull
	};

	enum class EDepthState
	{
		Default
	};

	enum class EBlendState
	{
		Default
	};

	struct GraphicsPipelineStateDesc
	{
		ERasterizerState rasterizer_state = ERasterizerState::Default;
		EBlendState blend_state = EBlendState::Default;
		EDepthState depth_state = EDepthState::Default;
		D3D12_PRIMITIVE_TOPOLOGY_TYPE topology_type = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		UINT num_render_targets = 0;
		DXGI_FORMAT rtv_formats[8];
		DXGI_FORMAT dsv_format = DXGI_FORMAT_UNKNOWN;
		InputLayout input_layout;
		ERootSignature root_signature;
		EShader VS;
		EShader PS;
		EShader DS;
		EShader HS;
		EShader GS;
	};

	class GraphicsPipelineState
	{
		static D3D12_RASTERIZER_DESC CreateRasterizerState(ERasterizerState state)
		{
			D3D12_RASTERIZER_DESC _state{};
			switch (state)
			{
			case ERasterizerState::Default:
			default:
				_state = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			}
			return _state;
		}
		static D3D12_DEPTH_STENCIL_DESC CreateDepthState(EDepthState state)
		{
			D3D12_DEPTH_STENCIL_DESC _state{};
			switch (state)
			{
			case EDepthState::Default:
			default:
				_state = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			}
			return _state;
		}
		static D3D12_BLEND_DESC CreateBlendState(EBlendState state)
		{
			D3D12_BLEND_DESC _state{};
			switch (state)
			{
			case EBlendState::Default:
			default:
				_state = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			}
			return _state;
		}
	public:
		GraphicsPipelineState(GraphicsDevice* gfx, GraphicsPipelineStateDesc const& desc)
			: desc(desc), gfx(gfx)
		{
			Create(desc);
			event_handle = ShaderManager::GetShaderRecompiledEvent().AddMember(&GraphicsPipelineState::OnShaderRecompiled, *this);
		}
		~GraphicsPipelineState()
		{
			ShaderManager::GetShaderRecompiledEvent().Remove(event_handle);
		}

		operator ID3D12PipelineState*() const
		{
			return pso.Get();
		}

	private:
		GraphicsDevice* gfx;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		GraphicsPipelineStateDesc desc;
		DelegateHandle event_handle;
	private:
		void OnShaderRecompiled(EShader s)
		{
			std::array<EShader, 5> shaders{ desc.VS, desc.PS, desc.GS, desc.HS, desc.DS };
			for (size_t i = 0; i < shaders.size(); ++i)
			{
				if (s == shaders[i])
				{
					Create(desc);
					return;
				}
			}
		}
		void Create(GraphicsPipelineStateDesc const& desc)
		{
			D3D12_GRAPHICS_PIPELINE_STATE_DESC _desc{};
			_desc.pRootSignature = RootSignatureCache::Get(desc.root_signature);
			_desc.VS = ShaderManager::GetShader(desc.VS);
			_desc.PS = ShaderManager::GetShader(desc.PS);
			_desc.GS = ShaderManager::GetShader(desc.GS);
			_desc.HS = ShaderManager::GetShader(desc.HS);
			_desc.DS = ShaderManager::GetShader(desc.DS);
			_desc.InputLayout = desc.input_layout;
			_desc.BlendState = CreateBlendState(desc.blend_state);
			_desc.RasterizerState = CreateRasterizerState(desc.rasterizer_state);
			_desc.DepthStencilState = CreateDepthState(desc.depth_state);
			_desc.SampleDesc = DXGI_SAMPLE_DESC{ .Count = 1, .Quality = 0 };
			_desc.DSVFormat = desc.dsv_format;
			_desc.NumRenderTargets = desc.num_render_targets;
			_desc.PrimitiveTopologyType = desc.topology_type;
			
			BREAK_IF_FAILED(gfx->GetDevice()->CreateGraphicsPipelineState(&_desc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())));
		}
	};

	struct ComputePipelineStateDesc
	{
		ERootSignature root_signature;
		EShader CS;
	};

	class ComputePipelineState
	{
	public:
		ComputePipelineState(GraphicsDevice* gfx, ComputePipelineStateDesc const& desc)
		{
			Create(desc);
			event_handle = ShaderManager::GetShaderRecompiledEvent().AddMember(&ComputePipelineState::OnShaderRecompiled, *this);
		}
		~ComputePipelineState()
		{
			ShaderManager::GetShaderRecompiledEvent().Remove(event_handle);
		}

	private:
		GraphicsDevice* gfx;
		Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
		ComputePipelineStateDesc desc;
		DelegateHandle event_handle;

	private:
		void OnShaderRecompiled(EShader s)
		{
			if (s == desc.CS) Create(desc);
		}
		void Create(ComputePipelineStateDesc const& desc)
		{
			D3D12_COMPUTE_PIPELINE_STATE_DESC _desc{};
			_desc.pRootSignature = RootSignatureCache::Get(desc.root_signature);
			_desc.CS = ShaderManager::GetShader(desc.CS);
			BREAK_IF_FAILED(gfx->GetDevice()->CreateComputePipelineState(&_desc, IID_PPV_ARGS(pso.ReleaseAndGetAddressOf())));
		}
	};
}