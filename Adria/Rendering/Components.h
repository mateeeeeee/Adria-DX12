#pragma once
#include <memory>
#include "Enums.h"
#include "../Core/Definitions.h"
#include <DirectXCollision.h>
#include "../Graphics/VertexTypes.h"
#include "../Graphics/VertexBuffer.h"
#include "../Graphics/IndexBuffer.h"
#include "../Graphics/TextureManager.h"
#include <unordered_map>


#define COMPONENT 

namespace adria
{
	struct COMPONENT Transform
	{
		DirectX::XMMATRIX starting_transform = DirectX::XMMatrixIdentity();
		DirectX::XMMATRIX current_transform = DirectX::XMMatrixIdentity();
	};

	struct COMPONENT Mesh
	{
		std::shared_ptr<VertexBuffer>	vb = nullptr;
		std::shared_ptr<IndexBuffer>	ib = nullptr;
		u32 indices_count = 0;
		u32 start_index_location = 0;
		u32 vertex_count = 0;
		u32 vertex_offset = 0;
		std::vector<CompleteVertex> vertices{};
		std::vector<u32> indices{};
		D3D12_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

		void Draw(ID3D12GraphicsCommandList* context) const
		{
			context->IASetPrimitiveTopology(topology);

			vb->Bind(context, 0);

			if (ib)
			{
				ib->Bind(context);
				context->DrawIndexedInstanced(indices_count,1, start_index_location, vertex_offset, 0);
			}
			else context->DrawInstanced(vertex_count,1, vertex_offset, 0);
		}

		void Draw(ID3D12GraphicsCommandList* context, D3D12_PRIMITIVE_TOPOLOGY override_topology) const
		{
			context->IASetPrimitiveTopology(override_topology);

			vb->Bind(context, 0);

			if (ib)
			{
				ib->Bind(context);
				context->DrawIndexedInstanced(indices_count, 1, start_index_location, vertex_offset, 0);
			}
			else context->DrawInstanced(vertex_count, 1, vertex_offset, 0);
		}
	};

	struct COMPONENT Material
	{
		TEXTURE_HANDLE normal_texture			  = INVALID_TEXTURE_HANDLE;
		TEXTURE_HANDLE albedo_texture		      = INVALID_TEXTURE_HANDLE;
		TEXTURE_HANDLE metallic_roughness_texture = INVALID_TEXTURE_HANDLE;
		TEXTURE_HANDLE metallic_texture			  = INVALID_TEXTURE_HANDLE;
		TEXTURE_HANDLE roughness_texture		  = INVALID_TEXTURE_HANDLE;
		TEXTURE_HANDLE emissive_texture			  = INVALID_TEXTURE_HANDLE;

		f32 albedo_factor		= 1.0f;
		f32 metallic_factor		= 1.0f;
		f32 roughness_factor	= 1.0f;
		f32 emissive_factor		= 1.0f;

		DirectX::XMFLOAT3 diffuse = DirectX::XMFLOAT3(1, 1, 1);
		PSO pso = PSO::eUnknown;
	};

	struct COMPONENT Light
	{
		DirectX::XMVECTOR position	= DirectX::XMVectorSet(0, 0, 0, 1);
		DirectX::XMVECTOR direction	= DirectX::XMVectorSet(0, -1, 0, 0);
		DirectX::XMVECTOR color		= DirectX::XMVectorSet(1, 1, 1, 1);
		f32 range = 100.0f;
		f32 energy = 1.0f;
		LightType type = LightType::eDirectional;
		f32 outer_cosine;
		f32 inner_cosine;
		bool casts_shadows = false;
		bool use_cascades = false;
		bool active = true;
		f32 volumetric_strength = 1.0f;
		bool volumetric = false;
		bool lens_flare = false;
		bool god_rays = false;
		f32 godrays_decay = 0.9f;
		f32 godrays_weight = 0.6f;
		f32 godrays_density = 0.78f;
		f32 godrays_exposure = 3.5f;
		bool screenspace_shadows = false;
	};

	struct COMPONENT Visibility
	{
		DirectX::BoundingBox aabb;
		bool camera_visible = true;
		bool light_visible = true;
	};

	struct COMPONENT Skybox
	{
		TEXTURE_HANDLE cubemap_texture;
		bool active;
	};
	
	struct COMPONENT Ocean {};

	struct COMPONENT Deferred {};

	struct COMPONENT Forward
	{
		bool transparent;
	};

	struct COMPONENT Tag
	{
		std::string name = "default";
	};

	struct COMPONENT RayTracing
	{

	};
}