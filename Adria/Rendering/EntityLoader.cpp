#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include "tiny_gltf.h"

#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "tiny_obj_loader.h"

#include "EntityLoader.h"
#include "Components.h"
#include "../Graphics/GraphicsDeviceDX12.h"
#include "../Logging/Logger.h"
#include "../Math/BoundingVolumeHelpers.h"
#include "../Math/ComputeTangentFrame.h"
#include "../Core/Definitions.h"
#include "../Utilities/StringUtil.h"
#include "../Utilities/FilesUtil.h"
#include "../Utilities/HashMap.h"
#include "../Utilities/Heightmap.h"


using namespace DirectX;


namespace adria
{

	std::vector<entt::entity> EntityLoader::LoadGrid(GridParameters const& params)
	{
		if (params.heightmap)
		{
			ADRIA_ASSERT(params.heightmap->Depth() == params.tile_count_z + 1);
			ADRIA_ASSERT(params.heightmap->Width() == params.tile_count_x + 1);
		}

		std::vector<entt::entity> chunks;
		std::vector<TexturedNormalVertex> vertices{};
		for (uint64 j = 0; j <= params.tile_count_z; j++)
		{
			for (uint64 i = 0; i <= params.tile_count_x; i++)
			{
				TexturedNormalVertex vertex{};

				float height = params.heightmap ? params.heightmap->HeightAt(i, j) : 0.0f;

				vertex.position = XMFLOAT3(i * params.tile_size_x, height, j * params.tile_size_z);
				vertex.uv = XMFLOAT2(i * 1.0f * params.texture_scale_x / (params.tile_count_x - 1), j * 1.0f * params.texture_scale_z / (params.tile_count_z - 1));
				vertex.normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
				vertices.push_back(vertex);
			}
		}

		if (!params.split_to_chunks)
		{
			std::vector<uint32> indices{};
			uint32 i1 = 0;
			uint32 i2 = 1;
			uint32 i3 = static_cast<uint32>(i1 + params.tile_count_x + 1);
			uint32 i4 = static_cast<uint32>(i2 + params.tile_count_x + 1);
			for (uint64 i = 0; i < params.tile_count_x * params.tile_count_z; ++i)
			{
				indices.push_back(i1);
				indices.push_back(i3);
				indices.push_back(i2);


				indices.push_back(i2);
				indices.push_back(i3);
				indices.push_back(i4);


				++i1;
				++i2;
				++i3;
				++i4;

				if (i1 % (params.tile_count_x + 1) == params.tile_count_x)
				{
					++i1;
					++i2;
					++i3;
					++i4;
				}
			}

			ComputeNormals(params.normal_type, vertices, indices);

			entt::entity grid = reg.create();

			BufferDesc vb_desc{
			.size = vertices.size() * sizeof(TexturedNormalVertex),
			.bind_flags = EBindFlag::None,
			.stride = sizeof(TexturedNormalVertex)
			};

			BufferDesc ib_desc{
				.size = indices.size() * sizeof(uint32),
				.bind_flags = EBindFlag::None,
				.stride = sizeof(uint32),
				.format = EFormat::R32_UINT
			};

			Mesh mesh{};
			mesh.indices_count = (uint32)indices.size();
			mesh.vertex_buffer = std::make_shared<Buffer>(gfx, vb_desc, vertices.data());
			mesh.index_buffer = std::make_shared<Buffer>(gfx, ib_desc, indices.data());

			reg.emplace<Mesh>(grid, mesh);
			reg.emplace<Transform>(grid);

			BoundingBox bounding_box = AABBFromRange(vertices.begin(), vertices.end());
			AABB aabb{};
			aabb.bounding_box = bounding_box;
			aabb.light_visible = true;
			aabb.camera_visible = true;
			aabb.UpdateBuffer(gfx);
			reg.emplace<AABB>(grid, aabb);
			chunks.push_back(grid);
		}
		else
		{
			std::vector<uint32> indices{};
			for (size_t j = 0; j < params.tile_count_z; j += params.chunk_count_z)
			{
				for (size_t i = 0; i < params.tile_count_x; i += params.chunk_count_x)
				{
					entt::entity chunk = reg.create();

					uint32 const indices_count = static_cast<uint32>(params.chunk_count_z * params.chunk_count_x * 3 * 2);
					uint32 const indices_offset = static_cast<uint32>(indices.size());


					std::vector<TexturedNormalVertex> chunk_vertices_aabb{};
					for (size_t k = j; k < j + params.chunk_count_z; ++k)
					{
						for (size_t m = i; m < i + params.chunk_count_x; ++m)
						{

							uint32 i1 = static_cast<uint32>(k * (params.tile_count_x + 1) + m);
							uint32 i2 = static_cast<uint32>(i1 + 1);
							uint32 i3 = static_cast<uint32>((k + 1) * (params.tile_count_x + 1) + m);
							uint32 i4 = static_cast<uint32>(i3 + 1);

							indices.push_back(i1);
							indices.push_back(i3);
							indices.push_back(i2);

							indices.push_back(i2);
							indices.push_back(i3);
							indices.push_back(i4);


							chunk_vertices_aabb.push_back(vertices[i1]);
							chunk_vertices_aabb.push_back(vertices[i2]);
							chunk_vertices_aabb.push_back(vertices[i3]);
							chunk_vertices_aabb.push_back(vertices[i4]);
						}
					}

					Mesh mesh{};
					mesh.indices_count = indices_count;
					mesh.start_index_location = indices_offset;

					reg.emplace<Mesh>(chunk, mesh);
					reg.emplace<Transform>(chunk);

					BoundingBox bounding_box = AABBFromRange(chunk_vertices_aabb.begin(), chunk_vertices_aabb.end());
					AABB aabb{};
					aabb.bounding_box = bounding_box;
					aabb.light_visible = true;
					aabb.camera_visible = true;
					aabb.UpdateBuffer(gfx);
					reg.emplace<AABB>(chunk, aabb);
					chunks.push_back(chunk);
				}
			}
			ComputeNormals(params.normal_type, vertices, indices);

			BufferDesc vb_desc{
			.size = vertices.size() * sizeof(TexturedNormalVertex),
			.bind_flags = EBindFlag::None,
			.stride = sizeof(TexturedNormalVertex)
			};

			BufferDesc ib_desc{
				.size = indices.size() * sizeof(uint32),
				.bind_flags = EBindFlag::None,
				.stride = sizeof(uint32),
				.format = EFormat::R32_UINT
			};

			std::shared_ptr<Buffer> vb = std::make_shared<Buffer>(gfx, vb_desc, vertices.data());
			std::shared_ptr<Buffer> ib = std::make_shared<Buffer>(gfx, ib_desc, indices.data());

			for (entt::entity chunk : chunks)
			{
				auto& mesh = reg.get<Mesh>(chunk);
				mesh.vertex_buffer = vb;
				mesh.index_buffer = ib;
			}
		}
		return chunks;
	}

	std::vector<entt::entity> EntityLoader::LoadObjMesh(std::string const& model_path)
	{
		tinyobj::ObjReaderConfig reader_config{};
		tinyobj::ObjReader reader;
		std::string model_name = GetFilename(model_path);
		if (!reader.ParseFromFile(model_path, reader_config))
		{
			if (!reader.Error().empty())
			{
				ADRIA_LOG(ERROR, reader.Error().c_str());
			}
			return {};
		}
		if (!reader.Warning().empty())
		{
			ADRIA_LOG(WARNING, reader.Warning().c_str());
		}

		tinyobj::attrib_t const& attrib = reader.GetAttrib();
		std::vector<tinyobj::shape_t> const& shapes = reader.GetShapes();
		
		// Loop over shapes
		std::vector<TexturedNormalVertex> vertices{};
		std::vector<uint32> indices{};
		std::vector<entt::entity> entities{};

		for (size_t s = 0; s < shapes.size(); s++)
		{
			entt::entity e = reg.create();
			entities.push_back(e);

			Mesh mesh_component{};
			mesh_component.start_index_location = static_cast<uint32>(indices.size());
			mesh_component.base_vertex_location = static_cast<uint32>(vertices.size());

			// Loop over faces(polygon)
			size_t index_offset = 0;
			for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
			{
				size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

				// Loop over vertices in the face.
				for (size_t v = 0; v < fv; v++)
				{
					// access to vertex
					tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
					indices.push_back((uint32)(index_offset + v));

					TexturedNormalVertex vertex{};
					tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
					tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
					tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

					vertex.position.x = vx;
					vertex.position.y = vy;
					vertex.position.z = vz;

					// Check if `normal_index` is zero or positive. negative = no normal data
					if (idx.normal_index >= 0)
					{
						tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
						tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
						tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

						vertex.normal.x = nx;
						vertex.normal.y = ny;
						vertex.normal.z = nz;
					}

					// Check if `texcoord_index` is zero or positive. negative = no texcoord data
					if (idx.texcoord_index >= 0)
					{
						tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
						tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

						vertex.uv.x = tx;
						vertex.uv.y = ty;
					}

					vertices.push_back(vertex);
				}
				index_offset += fv;

				// per-face material
				//shapes[s].mesh.material_ids[f];
			}
			mesh_component.indices_count = static_cast<uint32>(index_offset);
		}

		BufferDesc vb_desc{
			.size = vertices.size() * sizeof(CompleteVertex),
			.bind_flags = EBindFlag::None,
			.stride = sizeof(CompleteVertex)
		};

		BufferDesc ib_desc{
			.size = indices.size() * sizeof(uint32),
			.bind_flags = EBindFlag::None,
			.stride = sizeof(uint32),
			.format = EFormat::R32_UINT
		};

		std::shared_ptr<Buffer> vb = std::make_shared<Buffer>(gfx, vb_desc, vertices.data());
		std::shared_ptr<Buffer> ib = std::make_shared<Buffer>(gfx, ib_desc, indices.data());

		for (entt::entity e : entities)
		{
			auto& mesh = reg.get<Mesh>(e);
			mesh.vertex_buffer = vb;
			mesh.index_buffer = ib;
			reg.emplace<Tag>(e, model_name + " mesh" + std::to_string(entt::to_integral(e)));
		}

		ADRIA_LOG(INFO, "OBJ Mesh %s successfully loaded!", model_path.c_str());
		return entities;
	}

	EntityLoader::EntityLoader(entt::registry& reg, GraphicsDevice* gfx)
        : reg(reg), gfx(gfx)
    {
    }

	entt::entity EntityLoader::LoadSkybox(SkyboxParameters const& params)
    {
        entt::entity skybox = reg.create();

        Skybox sky{};
        sky.active = true;
		sky.used_in_rt = params.used_in_rt;

        if (params.cubemap.has_value()) sky.cubemap_texture = TextureManager::Get().LoadCubemap(params.cubemap.value());
        else sky.cubemap_texture = TextureManager::Get().LoadCubemap(params.cubemap_textures);

        reg.emplace<Skybox>(skybox, sky);
        reg.emplace<Tag>(skybox, "Skybox");
        return skybox;

    }
 
	entt::entity EntityLoader::LoadLight(LightParameters const& params)
    {
        entt::entity light = reg.create();

        if (params.light_data.type == ELightType::Directional)
            const_cast<LightParameters&>(params).light_data.position = XMVectorScale(-params.light_data.direction, 1e3);

        reg.emplace<Light>(light, params.light_data);
        if (params.mesh_type == ELightMesh::Quad)
        {
            uint32 const size = params.mesh_size;
            std::vector<TexturedVertex> const vertices =
            {
                { {-0.5f * size, -0.5f * size, 0.0f}, {0.0f, 0.0f}},
                { { 0.5f * size, -0.5f * size, 0.0f}, {1.0f, 0.0f}},
                { { 0.5f * size,  0.5f * size, 0.0f}, {1.0f, 1.0f}},
                { {-0.5f * size,  0.5f * size, 0.0f}, {0.0f, 1.0f}}
            };

            std::vector<uint16> const indices =
            {
                    0, 2, 1, 2, 0, 3
            };

			BufferDesc vb_desc{
			.size = vertices.size() * sizeof(TexturedVertex),
			.bind_flags = EBindFlag::None,
			.stride = sizeof(TexturedVertex)
			};

			BufferDesc ib_desc{
				.size = indices.size() * sizeof(uint16),
				.bind_flags = EBindFlag::None,
				.stride = sizeof(uint16),
				.format = EFormat::R16_UINT };

            Mesh mesh{};
            mesh.vertex_buffer = std::make_shared<Buffer>(gfx, vb_desc, vertices.data());
			mesh.index_buffer = std::make_shared<Buffer>(gfx, ib_desc, indices.data());
            mesh.indices_count = static_cast<uint32>(indices.size());

            reg.emplace<Mesh>(light, mesh);

            Material material{};
			XMFLOAT3 base_color;
			XMStoreFloat3(&base_color, params.light_data.color);
			material.base_color[0] = base_color.x;
			material.base_color[1] = base_color.y;
			material.base_color[2] = base_color.z;

            if (params.light_texture.has_value())
                material.albedo_texture = TextureManager::Get().LoadTexture(params.light_texture.value()); //
            else if (params.light_data.type == ELightType::Directional)
                material.albedo_texture = TextureManager::Get().LoadTexture(L"Resources/Textures/sun.png");

            if (params.light_data.type == ELightType::Directional)
                material.pso = EPipelineState::Sun;
			else 
			{ 
				ADRIA_LOG(ERROR, "Light with quad mesh needs to be directional!"); 
			}

            reg.emplace<Material>(light, material);
			auto translation_matrix = XMMatrixTranslationFromVector(params.light_data.position);
            reg.emplace<Transform>(light, translation_matrix, translation_matrix);
            if(params.light_data.type != ELightType::Directional) reg.emplace<Forward>(light, true);
        }
        else if (params.mesh_type == ELightMesh::Sphere)
        {
            //load sphere mesh and mesh component
           //Mesh sphere_mesh{};
           //
           //
           //Material material{};
           //XMStoreFloat3(&material.diffuse, params.light_data.color);
           //
           //if (params.light_texture.has_value())
           //    material.diffuse_texture = texture_manager.LoadTexture(params.light_texture.value()); //
           //else if (params.light_data.type == LightType::eDirectional)
           //    material.diffuse_texture = texture_manager.LoadTexture(L"Resources/Textures/sun.png");
           //
           //if (params.light_data.type == LightType::eDirectional)
           //    material.shader = StandardShader::eSun;
           //else if (material.diffuse_texture == INVALID_TEXTURE_HANDLE)
           //    material.shader = StandardShader::eSolid;
           //else material.shader = StandardShader::eTexture;
           //
           //reg.emplace<Material>(light, material);
        }

        switch (params.light_data.type)
        {
        case ELightType::Directional:
            reg.emplace<Tag>(light, "Directional Light");
            break;
        case ELightType::Spot:
            reg.emplace<Tag>(light, "Spot Light");
            break;
        case ELightType::Point:
            reg.emplace<Tag>(light, "Point Light");
            break;
        }
        return light;
    }

	std::vector<entt::entity> EntityLoader::LoadOcean(OceanParameters const& params)
	{
		std::vector<entt::entity> ocean_chunks = EntityLoader::LoadGrid(params.ocean_grid);

		Material ocean_material{};
		static float default_ocean_color[] = { 0.0123f, 0.3613f, 0.6867f };
		memcpy(ocean_material.base_color, default_ocean_color, 3 * sizeof(float));
		ocean_material.pso = EPipelineState::Unknown;
		Ocean ocean_component{};

		for (auto ocean_chunk : ocean_chunks)
		{
			reg.emplace<Material>(ocean_chunk, ocean_material);
			reg.emplace<Ocean>(ocean_chunk, ocean_component);
			reg.emplace<Tag>(ocean_chunk, "Ocean Chunk" + std::to_string(entt::to_integral(ocean_chunk)));
		}

		return ocean_chunks;
	}

	entt::entity EntityLoader::LoadDecal(DecalParameters const& params)
	{
		Decal decal{};
		TextureManager::Get().EnableMipMaps(false);
		if (!params.albedo_texture_path.empty()) decal.albedo_decal_texture = TextureManager::Get().LoadTexture(ToWideString(params.albedo_texture_path));
		if (!params.normal_texture_path.empty()) decal.normal_decal_texture = TextureManager::Get().LoadTexture(ToWideString(params.normal_texture_path));
		TextureManager::Get().EnableMipMaps(true);

		XMVECTOR P = XMLoadFloat4(&params.position);
		XMVECTOR N = XMLoadFloat4(&params.normal);

		XMVECTOR ProjectorDirection = XMVectorNegate(N);
		XMMATRIX RotationMatrix = XMMatrixRotationAxis(ProjectorDirection, params.rotation);
		XMMATRIX ModelMatrix = XMMatrixScaling(params.size, params.size, params.size) * RotationMatrix * XMMatrixTranslationFromVector(P);

		decal.decal_model_matrix = ModelMatrix;
		decal.modify_gbuffer_normals = params.modify_gbuffer_normals;

		XMFLOAT3 abs_normal;
		XMStoreFloat3(&abs_normal, XMVectorAbs(N));
		if (abs_normal.x >= abs_normal.y && abs_normal.x >= abs_normal.z)
		{
			decal.decal_type = EDecalType::Project_YZ;
		}
		else if (abs_normal.y >= abs_normal.x && abs_normal.y >= abs_normal.z)
		{
			decal.decal_type = EDecalType::Project_XZ;
		}
		else
		{
			decal.decal_type = EDecalType::Project_XY;
		}

		entt::entity decal_entity = reg.create();
		reg.emplace<Decal>(decal_entity, decal);
		if (params.name.empty()) reg.emplace<Tag>(decal_entity, "decal");
		else reg.emplace<Tag>(decal_entity, params.name);

		return decal_entity;
	}

	std::vector<entt::entity> EntityLoader::ImportModel_GLTF(ModelParameters const& params)
	{
		tinygltf::TinyGLTF loader;
		tinygltf::Model model;
		std::string err;
		std::string warn;
		bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, params.model_path);

		std::string model_name = GetFilename(params.model_path);
		if (!warn.empty())
		{
			ADRIA_LOG(WARNING, warn.c_str());
		}
		if (!err.empty())
		{
			ADRIA_LOG(ERROR, err.c_str());
			return {};
		}
		if (!ret)
		{
			ADRIA_LOG(ERROR, "Failed to load model %s", model_name.c_str());
			return {};
		}

		std::vector<CompleteVertex> vertices{};
		std::vector<uint32> indices{};
		std::vector<entt::entity> entities{};
		HashMap<std::string, std::vector<entt::entity>> mesh_name_to_entities_map;
		for (auto& mesh : model.meshes)
		{
			std::vector<entt::entity>& mesh_entities = mesh_name_to_entities_map[mesh.name];
			for (auto& primitive : mesh.primitives)
			{
				ADRIA_ASSERT(primitive.indices >= 0);
				tinygltf::Accessor const& index_accessor = model.accessors[primitive.indices];

				entt::entity e = reg.create();
				entities.push_back(e);
				mesh_entities.push_back(e);

				Material material{};
				tinygltf::Material gltf_material = model.materials[primitive.material];
				tinygltf::PbrMetallicRoughness pbr_metallic_roughness = gltf_material.pbrMetallicRoughness;
				if (pbr_metallic_roughness.baseColorTexture.index >= 0)
				{
					tinygltf::Texture const& base_texture = model.textures[pbr_metallic_roughness.baseColorTexture.index];
					tinygltf::Image const& base_image = model.images[base_texture.source];
					std::string texbase = params.textures_path + base_image.uri;
					material.albedo_texture = TextureManager::Get().LoadTexture(ToWideString(texbase));
					material.base_color[0] = (float)pbr_metallic_roughness.baseColorFactor[0];
					material.base_color[1] = (float)pbr_metallic_roughness.baseColorFactor[1];
					material.base_color[2] = (float)pbr_metallic_roughness.baseColorFactor[2];
				}
				if (pbr_metallic_roughness.metallicRoughnessTexture.index >= 0)
				{
					tinygltf::Texture const& metallic_roughness_texture = model.textures[pbr_metallic_roughness.metallicRoughnessTexture.index];
					tinygltf::Image const& metallic_roughness_image = model.images[metallic_roughness_texture.source];
					std::string texmetallicroughness = params.textures_path + metallic_roughness_image.uri;
					material.metallic_roughness_texture = TextureManager::Get().LoadTexture(ToWideString(texmetallicroughness));
					material.metallic_factor = (float)pbr_metallic_roughness.metallicFactor;
					material.roughness_factor = (float)pbr_metallic_roughness.roughnessFactor;
				}
				if (gltf_material.normalTexture.index >= 0)
				{
					tinygltf::Texture const& normal_texture = model.textures[gltf_material.normalTexture.index];
					tinygltf::Image const& normal_image = model.images[normal_texture.source];
					std::string texnormal = params.textures_path + normal_image.uri;
					material.normal_texture = TextureManager::Get().LoadTexture(ToWideString(texnormal));
				}
				if (gltf_material.emissiveTexture.index >= 0)
				{
					tinygltf::Texture const& emissive_texture = model.textures[gltf_material.emissiveTexture.index];
					tinygltf::Image const& emissive_image = model.images[emissive_texture.source];
					std::string texemissive = params.textures_path + emissive_image.uri;
					material.emissive_texture = TextureManager::Get().LoadTexture(ToWideString(texemissive));
					material.emissive_factor = (float)gltf_material.emissiveFactor[0];
				}
				material.pso = EPipelineState::GBuffer;
				material.alpha_cutoff = (float)gltf_material.alphaCutoff;
				material.double_sided = gltf_material.doubleSided;
				if (gltf_material.alphaMode == "OPAQUE")
				{
					material.alpha_mode = EMaterialAlphaMode::Opaque;
					material.pso = material.double_sided ? EPipelineState::GBuffer_NoCull : EPipelineState::GBuffer;
				}
				else if (gltf_material.alphaMode == "BLEND")
				{
					material.alpha_mode = EMaterialAlphaMode::Blend;
				}
				else if (gltf_material.alphaMode == "MASK")
				{
					material.alpha_mode = EMaterialAlphaMode::Mask;
					material.pso = material.double_sided ? EPipelineState::GBuffer_Mask_NoCull : EPipelineState::GBuffer_Mask;
				}
				reg.emplace<Material>(e, material);
				reg.emplace<Deferred>(e);

				Mesh mesh_component{};
				mesh_component.indices_count = static_cast<uint32>(index_accessor.count);
				mesh_component.start_index_location = static_cast<uint32>(indices.size());
				mesh_component.base_vertex_location = static_cast<uint32>(vertices.size());
				switch (primitive.mode)
				{
				case TINYGLTF_MODE_POINTS:
					mesh_component.topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
					break;
				case TINYGLTF_MODE_LINE:
					mesh_component.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
					break;
				case TINYGLTF_MODE_LINE_STRIP:
					mesh_component.topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
					break;
				case TINYGLTF_MODE_TRIANGLES:
					mesh_component.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
					break;
				case TINYGLTF_MODE_TRIANGLE_STRIP:
					mesh_component.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
					break;
				default:
					assert(false);
				}
				
				tinygltf::Accessor const& accessor = model.accessors[primitive.indices];
				tinygltf::BufferView const& bufferView = model.bufferViews[accessor.bufferView];
				tinygltf::Buffer const& buffer = model.buffers[bufferView.buffer];

				int stride = accessor.ByteStride(bufferView);
				uint32 vertex_offset = mesh_component.base_vertex_location;
				uint32 index_count = mesh_component.indices_count;
				uint32 index_offset = mesh_component.start_index_location;
				indices.reserve(indices.size() + index_count);
				unsigned char const* data = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;
				if (stride == 1)
				{
					for (size_t i = 0; i < index_count; i += 3)
					{
						indices.push_back(data[i + 0]);
						indices.push_back(data[i + 1]);
						indices.push_back(data[i + 2]);
					}
				}
				else if (stride == 2)
				{
					for (size_t i = 0; i < index_count; i += 3)
					{
						indices.push_back(((uint16_t*)data)[i + 0]);
						indices.push_back(((uint16_t*)data)[i + 1]);
						indices.push_back(((uint16_t*)data)[i + 2]);
					}
				}
				else if (stride == 4)
				{
					for (size_t i = 0; i < index_count; i += 3)
					{
						indices.push_back(((uint32_t*)data)[i + 0]);
						indices.push_back(((uint32_t*)data)[i + 1]);
						indices.push_back(((uint32_t*)data)[i + 2]);
					}
				}
				else ADRIA_ASSERT(false);

				std::vector<XMFLOAT3> positions;
				std::vector<XMFLOAT3> normals;
				std::vector<XMFLOAT2> uvs;
				std::vector<XMFLOAT3> tangents;
				std::vector<float>    tangent_handness;
				std::vector<XMFLOAT3> bitangents;
				for (auto& attr : primitive.attributes)
				{
					std::string const& attr_name = attr.first;
					int attr_data = attr.second;

					const tinygltf::Accessor& accessor = model.accessors[attr_data];
					const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
					const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

					int stride = accessor.ByteStride(bufferView);
					size_t vertex_count = accessor.count;
					const unsigned char* data = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;

					if (!attr_name.compare("POSITION"))
					{
						positions.reserve(vertex_count);
						for (size_t i = 0; i < vertex_count; ++i)
						{
							positions.push_back(*(XMFLOAT3*)((size_t)data + i * stride));
						}
					}
					else if (!attr_name.compare("NORMAL"))
					{
						normals.reserve(vertex_count);
						for (size_t i = 0; i < vertex_count; ++i)
						{
							normals.push_back(*(XMFLOAT3*)((size_t)data + i * stride));
							if (material.double_sided)
							{
								normals.back().x *= -1;
								normals.back().y *= -1;
								normals.back().z *= -1;
							}
						}
					}
					else if (!attr_name.compare("TANGENT"))
					{
						tangents.reserve(vertex_count);
						for (size_t i = 0; i < vertex_count; ++i)
						{
							XMFLOAT4 tangent = *(XMFLOAT4*)((size_t)data + i * stride);
							tangents.emplace_back(tangent.x, tangent.y, tangent.z);
							tangent_handness.push_back(tangent.w);
						}
					}
					else if (!attr_name.compare("TEXCOORD_0"))
					{
						uvs.reserve(vertex_count);
						if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT)
						{
							for (size_t i = 0; i < vertex_count; ++i)
							{
								XMFLOAT2 tex = *(XMFLOAT2*)((size_t)data + i * stride);
								tex.y = 1.0f - tex.y;
								uvs.push_back(tex);
							}
						}
						else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
						{
							for (size_t i = 0; i < vertex_count; ++i)
							{
								uint8 const& s = *(uint8*)((size_t)data + i * stride + 0 * sizeof(uint8));
								uint8 const& t = *(uint8*)((size_t)data + i * stride + 1 * sizeof(uint8));
								uvs.push_back(XMFLOAT2(s / 255.0f, 1.0f - t / 255.0f));
							}
						}
						else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
						{
							for (size_t i = 0; i < vertex_count; ++i)
							{
								uint16_t const& s = *(uint16*)((size_t)data + i * stride + 0 * sizeof(uint16));
								uint16_t const& t = *(uint16*)((size_t)data + i * stride + 1 * sizeof(uint16));
								uvs.push_back(XMFLOAT2(s / 65535.0f, 1.0f - t / 65535.0f));
							}
						}
					}
				}
				bool has_tangents = !tangents.empty();
				ADRIA_ASSERT(tangent_handness.size() == tangents.size());
				size_t vertex_count = positions.size();
				if (normals.size() != vertex_count) normals.resize(vertex_count);
				if (uvs.size() != vertex_count) uvs.resize(vertex_count);
				if (tangents.size() != vertex_count) tangents.resize(vertex_count);
				if (bitangents.size() != vertex_count) bitangents.resize(vertex_count);

				mesh_component.vertex_count = (uint32)vertex_count;
				reg.emplace<Mesh>(e, mesh_component);
				if (has_tangents)
				{
					for (size_t i = 0; i < vertex_count; ++i)
					{
						float tangent_w = tangent_handness[i];
						XMVECTOR _bitangent = XMVectorScale(XMVector3Cross(XMLoadFloat3(&normals[i]), XMLoadFloat3(&tangents[i])), tangent_w);
						XMStoreFloat3(&bitangents[i], XMVector3Normalize(_bitangent));
					}
				}

				vertices.reserve(vertices.size() + vertex_count);
				for (size_t i = 0; i < vertex_count; ++i)
				{
					vertices.emplace_back(
								positions[i],
								uvs[i],
								normals[i],
								XMFLOAT3(tangents[i].x, tangents[i].y, tangents[i].z),
								bitangents[i]
						);
				}
			}
		}

		std::function<void(int, XMMATRIX)> LoadNode;
		LoadNode = [&](int node_index, XMMATRIX parent_transform) 
		{
			if (node_index < 0) return;
			auto& node = model.nodes[node_index];
			struct Transforms
			{
				XMFLOAT4 rotation_local = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
				XMFLOAT3 scale_local = XMFLOAT3(1.0f, 1.0f, 1.0f);
				XMFLOAT3 translation_local = XMFLOAT3(0.0f, 0.0f, 0.0f);
				XMFLOAT4X4 world;
				bool update = true;
				void Update()
				{
					if (update)
					{
						XMVECTOR S_local = XMLoadFloat3(&scale_local);
						XMVECTOR R_local = XMLoadFloat4(&rotation_local);
						XMVECTOR T_local = XMLoadFloat3(&translation_local);
						XMMATRIX WORLD = XMMatrixScalingFromVector(S_local) *
							XMMatrixRotationQuaternion(R_local) *
							XMMatrixTranslationFromVector(T_local);
							XMStoreFloat4x4(&world, WORLD);
					}
				}
			} transforms;

			if (!node.scale.empty())
			{
				transforms.scale_local = XMFLOAT3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]);
			}
			if (!node.rotation.empty())
			{
				transforms.rotation_local = XMFLOAT4((float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2], (float)node.rotation[3]);
			}
			if (!node.translation.empty())
			{
				transforms.translation_local = XMFLOAT3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]);
			}
			if (!node.matrix.empty())
			{
				transforms.world._11 = (float)node.matrix[0];
				transforms.world._12 = (float)node.matrix[1];
				transforms.world._13 = (float)node.matrix[2];
				transforms.world._14 = (float)node.matrix[3];
				transforms.world._21 = (float)node.matrix[4];
				transforms.world._22 = (float)node.matrix[5];
				transforms.world._23 = (float)node.matrix[6];
				transforms.world._24 = (float)node.matrix[7];
				transforms.world._31 = (float)node.matrix[8];
				transforms.world._32 = (float)node.matrix[9];
				transforms.world._33 = (float)node.matrix[10];
				transforms.world._34 = (float)node.matrix[11];
				transforms.world._41 = (float)node.matrix[12];
				transforms.world._42 = (float)node.matrix[13];
				transforms.world._43 = (float)node.matrix[14];
				transforms.world._44 = (float)node.matrix[15];
				transforms.update = false;
			}
			transforms.Update();

			if (node.mesh >= 0)
			{
				auto const& mesh = model.meshes[node.mesh];
				std::vector<entt::entity> const& mesh_entities = mesh_name_to_entities_map[mesh.name];
				for (entt::entity e : mesh_entities)
				{
					Mesh const& mesh = reg.get<Mesh>(e);
					XMMATRIX model = XMLoadFloat4x4(&transforms.world) * parent_transform;
					BoundingBox bounding_box = AABBFromRange(vertices.begin() + mesh.base_vertex_location, vertices.begin() + mesh.base_vertex_location + mesh.vertex_count);
					bounding_box.Transform(bounding_box, model);

					AABB aabb{};
					aabb.bounding_box = bounding_box;
					aabb.light_visible = true;
					aabb.camera_visible = true;
					aabb.UpdateBuffer(gfx);
					reg.emplace<AABB>(e, aabb);
					reg.emplace<Transform>(e, model, model);
				}
			}

			for (int child : node.children) LoadNode(child, XMLoadFloat4x4(&transforms.world) * parent_transform);
		};
		tinygltf::Scene const& scene = model.scenes[std::max(0, model.defaultScene)];
		for (size_t i = 0; i < scene.nodes.size(); ++i)
		{
			LoadNode(scene.nodes[i], params.model_matrix);
		}

		BufferDesc vb_desc = VertexBufferDesc(vertices.size(), sizeof(CompleteVertex), params.used_in_raytracing);
		BufferDesc ib_desc = IndexBufferDesc(indices.size(), false, params.used_in_raytracing);

		std::shared_ptr<Buffer> vb = std::make_shared<Buffer>(gfx, vb_desc, vertices.data());
		std::shared_ptr<Buffer> ib = std::make_shared<Buffer>(gfx, ib_desc, indices.data());

		size_t rt_vertices_size = RayTracing::rt_vertices.size();
		size_t rt_indices_size = RayTracing::rt_indices.size();
		if (params.used_in_raytracing)
		{
			RayTracing::rt_vertices.insert(std::end(RayTracing::rt_vertices), std::begin(vertices), std::end(vertices));
			RayTracing::rt_indices.insert(std::end(RayTracing::rt_indices), std::begin(indices), std::end(indices));
		}

		entt::entity root = reg.create();
		reg.emplace<Transform>(root);
		reg.emplace<Tag>(root, model_name);
		Relationship relationship;
		relationship.children_count = entities.size();
		ADRIA_ASSERT(relationship.children_count <= Relationship::MAX_CHILDREN);
		for (size_t i = 0; i < relationship.children_count; ++i)
		{
			relationship.children[i] = entities[i];
		}
		reg.emplace<Relationship>(root, relationship);
		for (entt::entity e : entities)
		{
			auto& mesh = reg.get<Mesh>(e);
			mesh.vertex_buffer = vb;
			mesh.index_buffer = ib;
			reg.emplace<Tag>(e, model_name + " mesh" + std::to_string(entt::to_integral(e)));
			if (params.used_in_raytracing)
			{
				RayTracing rt_component{
					.vertex_offset = (uint32)rt_vertices_size + mesh.base_vertex_location,
					.index_offset = (uint32)rt_indices_size + mesh.start_index_location
				};
				reg.emplace<RayTracing>(e, rt_component);
			}
			reg.emplace<Relationship>(e, root);
		}
		ADRIA_LOG(INFO, "GLTF Mesh %s successfully loaded!", params.model_path.c_str());
		return entities;
	}
}