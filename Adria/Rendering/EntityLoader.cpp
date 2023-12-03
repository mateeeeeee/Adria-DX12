#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define TINYGLTF_NOEXCEPTION
#include "tiny_gltf.h"

#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "tiny_obj_loader.h"

#include "meshoptimizer.h"

#include "EntityLoader.h"
#include "Components.h"
#include "Meshlet.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Logging/Logger.h"
#include "Math/BoundingVolumeHelpers.h"
#include "Math/ComputeTangentFrame.h"
#include "Core/Paths.h"
#include "Utilities/StringUtil.h"
#include "Utilities/FilesUtil.h"
#include "Utilities/Heightmap.h"


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

				vertex.position = Vector3(i * params.tile_size_x, height, j * params.tile_size_z);
				vertex.uv = Vector2(i * 1.0f * params.texture_scale_x / (params.tile_count_x - 1), j * 1.0f * params.texture_scale_z / (params.tile_count_z - 1));
				vertex.normal = Vector3(0.0f, 1.0f, 0.0f);
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

			GfxBufferDesc vb_desc{
			.size = vertices.size() * sizeof(TexturedNormalVertex),
			.bind_flags = GfxBindFlag::None,
			.stride = sizeof(TexturedNormalVertex)
			};

			GfxBufferDesc ib_desc{
				.size = indices.size() * sizeof(uint32),
				.bind_flags = GfxBindFlag::None,
				.stride = sizeof(uint32),
				.format = GfxFormat::R32_UINT
			};

			SubMesh submesh{};
			submesh.indices_count = (uint32)indices.size();
			submesh.vertex_buffer = std::make_shared<GfxBuffer>(gfx, vb_desc, vertices.data());
			submesh.index_buffer = std::make_shared<GfxBuffer>(gfx, ib_desc, indices.data());
			submesh.bounding_box = AABBFromRange(vertices.begin(), vertices.end());
			reg.emplace<SubMesh>(grid, submesh);
			reg.emplace<Transform>(grid);

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

					SubMesh submesh{};
					submesh.indices_count = indices_count;
					submesh.start_index_location = indices_offset;
					submesh.bounding_box = AABBFromRange(chunk_vertices_aabb.begin(), chunk_vertices_aabb.end());
					reg.emplace<SubMesh>(chunk, submesh);
					reg.emplace<Transform>(chunk);
					chunks.push_back(chunk);
				}
			}
			ComputeNormals(params.normal_type, vertices, indices);

			GfxBufferDesc vb_desc{
			.size = vertices.size() * sizeof(TexturedNormalVertex),
			.bind_flags = GfxBindFlag::None,
			.stride = sizeof(TexturedNormalVertex)
			};

			GfxBufferDesc ib_desc{
				.size = indices.size() * sizeof(uint32),
				.bind_flags = GfxBindFlag::None,
				.stride = sizeof(uint32),
				.format = GfxFormat::R32_UINT
			};

			std::shared_ptr<GfxBuffer> vb = std::make_shared<GfxBuffer>(gfx, vb_desc, vertices.data());
			std::shared_ptr<GfxBuffer> ib = std::make_shared<GfxBuffer>(gfx, ib_desc, indices.data());

			for (entt::entity chunk : chunks)
			{
				auto& submesh = reg.get<SubMesh>(chunk);
				submesh.vertex_buffer = vb;
				submesh.index_buffer = ib;
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

			SubMesh mesh_component{};
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

		GfxBufferDesc vb_desc{
			.size = vertices.size() * sizeof(CompleteVertex),
			.bind_flags = GfxBindFlag::None,
			.stride = sizeof(CompleteVertex)
		};

		GfxBufferDesc ib_desc{
			.size = indices.size() * sizeof(uint32),
			.bind_flags = GfxBindFlag::None,
			.stride = sizeof(uint32),
			.format = GfxFormat::R32_UINT
		};

		std::shared_ptr<GfxBuffer> vb = std::make_shared<GfxBuffer>(gfx, vb_desc, vertices.data());
		std::shared_ptr<GfxBuffer> ib = std::make_shared<GfxBuffer>(gfx, ib_desc, indices.data());

		for (entt::entity e : entities)
		{
			auto& mesh = reg.get<SubMesh>(e);
			mesh.vertex_buffer = vb;
			mesh.index_buffer = ib;
			reg.emplace<Tag>(e, model_name + " mesh" + std::to_string(entt::to_integral(e)));
		}

		ADRIA_LOG(INFO, "OBJ Mesh %s successfully loaded!", model_path.c_str());
		return entities;
	}

	EntityLoader::EntityLoader(entt::registry& reg, GfxDevice* gfx)
        : reg(reg), gfx(gfx)
    {
		g_GeometryBufferCache.Initialize(gfx);
    }

	EntityLoader::~EntityLoader()
	{
		g_GeometryBufferCache.Destroy();
	}

	entt::entity EntityLoader::LoadSkybox(SkyboxParameters const& params)
    {
        entt::entity skybox = reg.create();

        Skybox sky{};
        sky.active = true;

        if (params.cubemap.has_value()) sky.cubemap_texture = g_TextureManager.LoadTexture(params.cubemap.value());
        else sky.cubemap_texture = g_TextureManager.LoadCubemap(params.cubemap_textures);

        reg.emplace<Skybox>(skybox, sky);
        reg.emplace<Tag>(skybox, "Skybox");
        return skybox;

    }

	entt::entity EntityLoader::LoadLight(LightParameters const& params)
    {
        entt::entity light = reg.create();

        if (params.light_data.type == LightType::Directional)
            const_cast<LightParameters&>(params).light_data.position = -params.light_data.direction * 1e3;

        reg.emplace<Light>(light, params.light_data);
        if (params.mesh_type == LightMesh::Quad)
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

			GfxBufferDesc vb_desc{
			.size = vertices.size() * sizeof(TexturedVertex),
			.bind_flags = GfxBindFlag::None,
			.stride = sizeof(TexturedVertex)
			};

			GfxBufferDesc ib_desc{
				.size = indices.size() * sizeof(uint16),
				.bind_flags = GfxBindFlag::None,
				.stride = sizeof(uint16),
				.format = GfxFormat::R16_UINT };

            SubMesh submesh{};
            submesh.vertex_buffer = std::make_shared<GfxBuffer>(gfx, vb_desc, vertices.data());
			submesh.index_buffer = std::make_shared<GfxBuffer>(gfx, ib_desc, indices.data());
            submesh.indices_count = static_cast<uint32>(indices.size());

            reg.emplace<SubMesh>(light, submesh);

            Material material{};
			material.base_color[0] = params.light_data.color.x;
			material.base_color[1] = params.light_data.color.y;
			material.base_color[2] = params.light_data.color.z;

            if (params.light_texture.has_value())
                material.albedo_texture = g_TextureManager.LoadTexture(params.light_texture.value()); //
            else if (params.light_data.type == LightType::Directional)
                material.albedo_texture = g_TextureManager.LoadTexture(paths::TexturesDir() + "sun.dds");

            reg.emplace<Material>(light, material);
			Matrix translation_matrix = Matrix::CreateTranslation(Vector3(&params.light_data.position.x));
            reg.emplace<Transform>(light, translation_matrix);
        }
        else if (params.mesh_type == LightMesh::Sphere)
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
           //    material.diffuse_texture = texture_manager.LoadTexture(paths::TexturesDir() + "sun.png");
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
        case LightType::Directional:
            reg.emplace<Tag>(light, "Directional Light");
            break;
        case LightType::Spot:
            reg.emplace<Tag>(light, "Spot Light");
            break;
        case LightType::Point:
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
		g_TextureManager.EnableMipMaps(false);
		if (!params.albedo_texture_path.empty()) decal.albedo_decal_texture = g_TextureManager.LoadTexture(params.albedo_texture_path);
		else decal.albedo_decal_texture = g_TextureManager.LoadTexture(paths::TexturesDir() + "Decals/Decal_00_Albedo.tga");
		if (!params.normal_texture_path.empty()) decal.normal_decal_texture = g_TextureManager.LoadTexture(params.normal_texture_path);
		else decal.normal_decal_texture = g_TextureManager.LoadTexture(paths::TexturesDir() + "Decals/Decal_00_Normal.tga");
		g_TextureManager.EnableMipMaps(true);

		Vector3 P = params.position;
		Vector3 N = params.normal;
		Vector3 ProjectorDirection = -N;
		Matrix RotationMatrix = Matrix::CreateFromAxisAngle(ProjectorDirection, params.rotation);
		Matrix ModelMatrix = Matrix::CreateScale(params.size) * RotationMatrix * Matrix::CreateTranslation(P);

		decal.decal_model_matrix = ModelMatrix;
		decal.modify_gbuffer_normals = params.modify_gbuffer_normals;

		Vector3 abs_normal = XMVectorAbs(N);
		if (abs_normal.x >= abs_normal.y && abs_normal.x >= abs_normal.z)
		{
			decal.decal_type = DecalType::Project_YZ;
		}
		else if (abs_normal.y >= abs_normal.x && abs_normal.y >= abs_normal.z)
		{
			decal.decal_type = DecalType::Project_XZ;
		}
		else
		{
			decal.decal_type = DecalType::Project_XY;
		}

		entt::entity decal_entity = reg.create();
		reg.emplace<Decal>(decal_entity, decal);
		if (params.name.empty()) reg.emplace<Tag>(decal_entity, "decal");
		else reg.emplace<Tag>(decal_entity, params.name);

		return decal_entity;
	}

	entt::entity EntityLoader::ImportModel_GLTF(ModelParameters const& params)
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
		entt::entity mesh_entity = reg.create();
		Mesh mesh{};

		//process the materials
		mesh.materials.reserve(model.materials.size());
		for (auto const& gltf_material : model.materials)
		{
			Material& material = mesh.materials.emplace_back();
			material.alpha_cutoff = (float)gltf_material.alphaCutoff;
			material.double_sided = gltf_material.doubleSided;

			if (params.force_mask_alpha_usage)
			{
				material.alpha_mode = MaterialAlphaMode::Mask;
			}
			if (gltf_material.alphaMode == "OPAQUE")
			{
				material.alpha_mode = MaterialAlphaMode::Opaque;
			}
			else if (gltf_material.alphaMode == "BLEND")
			{
				material.alpha_mode = MaterialAlphaMode::Blend;
			}
			else if (gltf_material.alphaMode == "MASK")
			{
				material.alpha_mode = MaterialAlphaMode::Mask;
			}
			tinygltf::PbrMetallicRoughness pbr_metallic_roughness = gltf_material.pbrMetallicRoughness;
			material.base_color[0] = (float)pbr_metallic_roughness.baseColorFactor[0];
			material.base_color[1] = (float)pbr_metallic_roughness.baseColorFactor[1];
			material.base_color[2] = (float)pbr_metallic_roughness.baseColorFactor[2];
			material.metallic_factor = (float)pbr_metallic_roughness.metallicFactor;
			material.roughness_factor = (float)pbr_metallic_roughness.roughnessFactor;
			material.emissive_factor = (float)gltf_material.emissiveFactor[0];

			if (pbr_metallic_roughness.baseColorTexture.index >= 0)
			{
				tinygltf::Texture const& base_texture = model.textures[pbr_metallic_roughness.baseColorTexture.index];
				tinygltf::Image const& base_image = model.images[base_texture.source];
				std::string texbase = params.textures_path + base_image.uri;

				material.albedo_texture = g_TextureManager.LoadTexture(texbase);
			}
			if (pbr_metallic_roughness.metallicRoughnessTexture.index >= 0)
			{
				tinygltf::Texture const& metallic_roughness_texture = model.textures[pbr_metallic_roughness.metallicRoughnessTexture.index];
				tinygltf::Image const& metallic_roughness_image = model.images[metallic_roughness_texture.source];
				std::string texmetallicroughness = params.textures_path + metallic_roughness_image.uri;
				material.metallic_roughness_texture = g_TextureManager.LoadTexture(texmetallicroughness);
				
			}
			if (gltf_material.normalTexture.index >= 0)
			{
				tinygltf::Texture const& normal_texture = model.textures[gltf_material.normalTexture.index];
				tinygltf::Image const& normal_image = model.images[normal_texture.source];
				std::string texnormal = params.textures_path + normal_image.uri;
				material.normal_texture = g_TextureManager.LoadTexture(texnormal);
			}
			if (gltf_material.emissiveTexture.index >= 0)
			{
				tinygltf::Texture const& emissive_texture = model.textures[gltf_material.emissiveTexture.index];
				tinygltf::Image const& emissive_image = model.images[emissive_texture.source];
				std::string texemissive = params.textures_path + emissive_image.uri;
				material.emissive_texture = g_TextureManager.LoadTexture(texemissive);
			}
		}

		std::unordered_map<int32, std::vector<int32>> mesh_primitives_map; //mesh index -> vector of primitive indices
		int32 primitive_count = 0;

		struct MeshData
		{
			DirectX::BoundingBox bounding_box;
			int32 material_index = -1;
			GfxPrimitiveTopology topology = GfxPrimitiveTopology::TriangleList;

			std::vector<Vector3> positions_stream;
			std::vector<Vector3> normals_stream;
			std::vector<Vector4> tangents_stream;
			std::vector<Vector2> uvs_stream;
			std::vector<uint32>   indices;

			std::vector<Meshlet>		 meshlets;
			std::vector<uint32>			 meshlet_vertices;
			std::vector<MeshletTriangle> meshlet_triangles;
		};
		std::vector<MeshData> mesh_datas{};
		for (int32 i = 0; i < model.meshes.size(); ++i)
		{
			std::vector<int32>& primitives = mesh_primitives_map[i];
			auto const& gltf_mesh = model.meshes[i];
			for (auto const& gltf_primitive : gltf_mesh.primitives)
			{
				ADRIA_ASSERT(gltf_primitive.indices >= 0);
				tinygltf::Accessor const& index_accessor = model.accessors[gltf_primitive.indices];
				tinygltf::BufferView const& buffer_view = model.bufferViews[index_accessor.bufferView];
				tinygltf::Buffer const& buffer = model.buffers[buffer_view.buffer];

				MeshData& mesh_data = mesh_datas.emplace_back();
				mesh_data.material_index = gltf_primitive.material;

				mesh_data.indices.reserve(index_accessor.count);
				auto AddIndices = [&]<typename T>()
				{
					T* data = (T*)(buffer.data.data() + index_accessor.byteOffset + buffer_view.byteOffset);
					uint32 triangle_cw[]   = { 0, 1, 2 };
					uint32 triangle_ccw[]  = { 0, 2, 1 };
					uint32* order = params.triangle_ccw ? triangle_ccw : triangle_cw;
					for (size_t i = 0; i < index_accessor.count; i += 3)
					{
						mesh_data.indices.push_back(data[i + order[0]]);
						mesh_data.indices.push_back(data[i + order[1]]);
						mesh_data.indices.push_back(data[i + order[2]]);
					}
				};
				int stride = index_accessor.ByteStride(buffer_view);
				switch (stride)
				{
				case 1:
					AddIndices.template operator()<uint8>();
					break;
				case 2:
					AddIndices.template operator()<uint16>();
					break;
				case 4:
					AddIndices.template operator()<uint32>();
					break;
				default:
					ADRIA_ASSERT(false);
				}
				switch (gltf_primitive.mode)
				{
				case TINYGLTF_MODE_POINTS:
					mesh_data.topology = GfxPrimitiveTopology::PointList;
					break;
				case TINYGLTF_MODE_LINE:
					mesh_data.topology = GfxPrimitiveTopology::LineList;
					break;
				case TINYGLTF_MODE_LINE_STRIP:
					mesh_data.topology = GfxPrimitiveTopology::LineStrip;
					break;
				case TINYGLTF_MODE_TRIANGLES:
					mesh_data.topology = GfxPrimitiveTopology::TriangleList;
					break;
				case TINYGLTF_MODE_TRIANGLE_STRIP:
					mesh_data.topology = GfxPrimitiveTopology::TriangleStrip;
					break;
				default:
					ADRIA_ASSERT(false);
				}

				for (auto const& attr : gltf_primitive.attributes)
				{
					std::string const& attr_name = attr.first;
					int attr_data = attr.second;

					const tinygltf::Accessor& accessor = model.accessors[attr_data];
					const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
					const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

					int stride = accessor.ByteStride(buffer_view);
					size_t count = accessor.count;
					const unsigned char* data = buffer.data.data() + accessor.byteOffset + buffer_view.byteOffset;

					auto ReadAttributeData = [&]<typename T>(std::vector<T>&stream, const char* stream_name)
					{
						if (!attr_name.compare(stream_name))
						{
							stream.reserve(count);
							for (size_t i = 0; i < count; ++i)
							{
								stream.push_back(*(T*)((size_t)data + i * stride));
							}
						}
					};
					ReadAttributeData(mesh_data.positions_stream, "POSITION");
					ReadAttributeData(mesh_data.normals_stream, "NORMAL");
					ReadAttributeData(mesh_data.tangents_stream, "TANGENT");
					ReadAttributeData(mesh_data.uvs_stream, "TEXCOORD_0");
				}
				primitives.push_back(primitive_count++);
			}
		}

		uint64 total_buffer_size = 0;
		for (auto& mesh_data : mesh_datas)
		{
			std::vector<uint32> const& indices = mesh_data.indices;
			size_t vertex_count = mesh_data.positions_stream.size();

			bool has_tangents = !mesh_data.tangents_stream.empty();
			if (mesh_data.normals_stream.size() != vertex_count) mesh_data.normals_stream.resize(vertex_count);
			if (mesh_data.uvs_stream.size() != vertex_count) mesh_data.uvs_stream.resize(vertex_count);
			if (mesh_data.tangents_stream.size() != vertex_count) mesh_data.tangents_stream.resize(vertex_count);

			meshopt_optimizeVertexCache(mesh_data.indices.data(), mesh_data.indices.data(), mesh_data.indices.size(), vertex_count);
			meshopt_optimizeOverdraw(mesh_data.indices.data(), mesh_data.indices.data(), mesh_data.indices.size(), &mesh_data.positions_stream[0].x, vertex_count, sizeof(Vector3), 1.05f);
			std::vector<uint32> remap(vertex_count);
			meshopt_optimizeVertexFetchRemap(&remap[0], mesh_data.indices.data(), mesh_data.indices.size(), vertex_count);
			meshopt_remapIndexBuffer(mesh_data.indices.data(), mesh_data.indices.data(), mesh_data.indices.size(), &remap[0]);
			meshopt_remapVertexBuffer(mesh_data.positions_stream.data(), mesh_data.positions_stream.data(), vertex_count, sizeof(Vector3), &remap[0]);
			meshopt_remapVertexBuffer(mesh_data.normals_stream.data(), mesh_data.normals_stream.data(), mesh_data.normals_stream.size(), sizeof(Vector3), &remap[0]);
			meshopt_remapVertexBuffer(mesh_data.tangents_stream.data(), mesh_data.tangents_stream.data(), mesh_data.tangents_stream.size(), sizeof(Vector4), &remap[0]);
			meshopt_remapVertexBuffer(mesh_data.uvs_stream.data(), mesh_data.uvs_stream.data(), mesh_data.uvs_stream.size(), sizeof(Vector2), &remap[0]);

			size_t const max_meshlets = meshopt_buildMeshletsBound(mesh_data.indices.size(), MESHLET_MAX_VERTICES, MESHLET_MAX_TRIANGLES);
			mesh_data.meshlets.resize(max_meshlets);
			mesh_data.meshlet_vertices.resize(max_meshlets * MESHLET_MAX_VERTICES);

			std::vector<unsigned char> meshlet_triangles(max_meshlets * MESHLET_MAX_TRIANGLES * 3);
			std::vector<meshopt_Meshlet> meshlets(max_meshlets);

			size_t meshlet_count = meshopt_buildMeshlets(meshlets.data(), mesh_data.meshlet_vertices.data(), meshlet_triangles.data(),
				mesh_data.indices.data(), mesh_data.indices.size(), &mesh_data.positions_stream[0].x, mesh_data.positions_stream.size(), sizeof(Vector3),
				MESHLET_MAX_VERTICES, MESHLET_MAX_TRIANGLES, 0);

			meshopt_Meshlet const& last = meshlets[meshlet_count - 1];
			meshlet_triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
			meshlets.resize(meshlet_count);

			mesh_data.meshlets.resize(meshlet_count);
			mesh_data.meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
			mesh_data.meshlet_triangles.resize(meshlet_triangles.size() / 3);

			uint32 triangle_offset = 0;
			for (size_t i = 0; i < meshlet_count; ++i)
			{
				meshopt_Meshlet const& m = meshlets[i];
				meshopt_Bounds meshopt_bounds = meshopt_computeMeshletBounds(&mesh_data.meshlet_vertices[m.vertex_offset], &meshlet_triangles[m.triangle_offset],
					m.triangle_count, reinterpret_cast<float const*>(mesh_data.positions_stream.data()), vertex_count, sizeof(Vector3));

				unsigned char* src_triangles = meshlet_triangles.data() + m.triangle_offset;
				for (uint32 triangle_idx = 0; triangle_idx < m.triangle_count; ++triangle_idx)
				{
					MeshletTriangle& tri = mesh_data.meshlet_triangles[triangle_idx + triangle_offset];
					tri.V0 = *src_triangles++;
					tri.V1 = *src_triangles++;
					tri.V2 = *src_triangles++;
				}

				Meshlet& meshlet = mesh_data.meshlets[i];
				std::memcpy(meshlet.center, meshopt_bounds.center, sizeof(float) * 3);

				meshlet.radius = meshopt_bounds.radius;
				meshlet.vertex_count = m.vertex_count;
				meshlet.triangle_count = m.triangle_count;
				meshlet.vertex_offset = m.vertex_offset;
				meshlet.triangle_offset = triangle_offset;
				triangle_offset += m.triangle_count;

			}
			mesh_data.meshlet_triangles.resize(triangle_offset);

			total_buffer_size += Align(mesh_data.indices.size() * sizeof(uint32), 16);
			total_buffer_size += Align(mesh_data.positions_stream.size() * sizeof(Vector3), 16);
			total_buffer_size += Align(mesh_data.uvs_stream.size() * sizeof(Vector2), 16);
			total_buffer_size += Align(mesh_data.normals_stream.size() * sizeof(Vector3), 16);
			total_buffer_size += Align(mesh_data.tangents_stream.size() * sizeof(Vector4), 16);
			total_buffer_size += Align(mesh_data.meshlets.size() * sizeof(Meshlet), 16);
			total_buffer_size += Align(mesh_data.meshlet_vertices.size() * sizeof(uint32), 16);
			total_buffer_size += Align(mesh_data.meshlet_triangles.size() * sizeof(MeshletTriangle), 16);

			mesh_data.bounding_box = AABBFromPositions(mesh_data.positions_stream);
		}

		GfxDynamicAllocation staging_buffer = gfx->GetDynamicAllocator()->Allocate(total_buffer_size, 16);

		uint32 current_offset = 0;
		auto CopyData = [&staging_buffer, &current_offset]<typename T>(std::vector<T> const& _data)
		{
			uint64 current_copy_size = _data.size() * sizeof(T);
			staging_buffer.Update(_data.data(), current_copy_size, current_offset);
			current_offset += (uint32)Align(current_copy_size, 16);
		};

		mesh.submeshes.reserve(mesh_datas.size());
		for (size_t i = 0; i < mesh_datas.size(); ++i)
		{
			auto const& mesh_data = mesh_datas[i];

			SubMeshGPU& submesh = mesh.submeshes.emplace_back();

			submesh.indices_offset = current_offset;
			submesh.indices_count = (uint32)mesh_data.indices.size();
			CopyData(mesh_data.indices);

			submesh.vertices_count = (uint32)mesh_data.positions_stream.size();
			submesh.positions_offset = current_offset;
			CopyData(mesh_data.positions_stream);

			submesh.uvs_offset = current_offset;
			CopyData(mesh_data.uvs_stream);

			submesh.normals_offset = current_offset;
			CopyData(mesh_data.normals_stream);

			submesh.tangents_offset = current_offset;
			CopyData(mesh_data.tangents_stream);

			submesh.meshlet_offset = current_offset;
			CopyData(mesh_data.meshlets);

			submesh.meshlet_vertices_offset = current_offset;
			CopyData(mesh_data.meshlet_vertices);

			submesh.meshlet_triangles_offset = current_offset;
			CopyData(mesh_data.meshlet_triangles);

			submesh.meshlet_count = (uint32)mesh_data.meshlets.size();

			submesh.bounding_box = mesh_data.bounding_box;
			submesh.topology = mesh_data.topology;
			submesh.material_index = mesh_data.material_index;
		}
		mesh.geometry_buffer_handle = g_GeometryBufferCache.CreateAndInitializeGeometryBuffer(staging_buffer.buffer, total_buffer_size, staging_buffer.offset);

		std::function<void(int, Matrix const&)> LoadNode;
		LoadNode = [&](int node_index, Matrix const& parent_transform)
		{
			if (node_index < 0) return;
			auto& node = model.nodes[node_index];
			struct Transforms
			{
				Vector4 rotation_local = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
				Vector3 scale_local = Vector3(1.0f, 1.0f, 1.0f);
				Vector3 translation_local = Vector3(0.0f, 0.0f, 0.0f);
				Matrix world = Matrix::Identity;
				bool update = true;
				void Update()
				{
					if (update)
					{
							world = Matrix::CreateScale(scale_local) *
							Matrix::CreateFromQuaternion(rotation_local) *
							Matrix::CreateTranslation(translation_local);
					}
				}
			} transforms;

			if (!node.scale.empty())
			{
				transforms.scale_local = Vector3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]);
			}
			if (!node.rotation.empty())
			{
				transforms.rotation_local = Vector4((float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2], (float)node.rotation[3]);
			}
			if (!node.translation.empty())
			{
				transforms.translation_local = Vector3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]);
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
				Matrix model_matrix = transforms.world * parent_transform;
				for (auto primitive : mesh_primitives_map[node.mesh])
				{
					SubMeshInstance& instance = mesh.instances.emplace_back();
					instance.submesh_index = primitive;
					instance.world_transform = model_matrix;
					instance.parent = mesh_entity;
				}
			}

			for (int child : node.children) LoadNode(child, transforms.world * parent_transform);
		};

		tinygltf::Scene const& scene = model.scenes[std::max(0, model.defaultScene)];
		for (size_t i = 0; i < scene.nodes.size(); ++i)
		{
			LoadNode(scene.nodes[i], params.model_matrix);
		}

		reg.emplace<Mesh>(mesh_entity, mesh);
		reg.emplace<Tag>(mesh_entity, model_name + " mesh");

		if (gfx->GetCapabilities().SupportsRayTracing()) reg.emplace<RayTracing>(mesh_entity);

		ADRIA_LOG(INFO, "GLTF Mesh %s successfully loaded!", params.model_path.c_str());
		return mesh_entity;
	}
}