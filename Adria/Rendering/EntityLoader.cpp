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
#include "../Graphics/GraphicsCoreDX12.h"
#include "../Logging/Logger.h"
#include "../Math/BoundingVolumeHelpers.h"
#include "../Math/ComputeTangentFrame.h"
#include "../Core/Definitions.h"
#include "../Utilities/StringUtil.h"
#include "../Utilities/FilesUtil.h"


using namespace DirectX;


namespace adria
{
    using namespace tecs;



	std::vector<entity> EntityLoader::LoadGrid(grid_parameters_t const& params)
	{
		if (params.heightmap)
		{
			ADRIA_ASSERT(params.heightmap->Depth() == params.tile_count_z + 1);
			ADRIA_ASSERT(params.heightmap->Width() == params.tile_count_x + 1);
		}

		std::vector<entity> chunks;
		std::vector<TexturedNormalVertex> vertices{};
		for (uint64 j = 0; j <= params.tile_count_z; j++)
		{
			for (uint64 i = 0; i <= params.tile_count_x; i++)
			{
				TexturedNormalVertex vertex{};

				float32 height = params.heightmap ? params.heightmap->HeightAt(i, j) : 0.0f;

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

			entity grid = reg.create();

			Mesh mesh{};
			mesh.indices_count = (uint32)indices.size();
			mesh.vertex_buffer = std::make_shared<VertexBuffer>(gfx, vertices);
			mesh.index_buffer = std::make_shared<IndexBuffer>(gfx, indices);

			reg.emplace<Mesh>(grid, mesh);
			reg.emplace<Transform>(grid);

			BoundingBox aabb = AABBFromRange(vertices.begin(), vertices.end());
			reg.emplace<Visibility>(grid, aabb, true, true);

			chunks.push_back(grid);
		}
		else
		{
			std::vector<uint32> indices{};
			for (size_t j = 0; j < params.tile_count_z; j += params.chunk_count_z)
			{
				for (size_t i = 0; i < params.tile_count_x; i += params.chunk_count_x)
				{
					entity chunk = reg.create();

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

					BoundingBox aabb = AABBFromRange(chunk_vertices_aabb.begin(), chunk_vertices_aabb.end());

					reg.emplace<Visibility>(chunk, aabb, true, true);

					chunks.push_back(chunk);

				}
			}

			ComputeNormals(params.normal_type, vertices, indices);

			std::shared_ptr<VertexBuffer> vb = std::make_shared<VertexBuffer>(gfx, vertices);
			std::shared_ptr<IndexBuffer> ib = std::make_shared<IndexBuffer>(gfx, indices);

			for (entity chunk : chunks)
			{
				auto& mesh = reg.get<Mesh>(chunk);

				mesh.vertex_buffer = vb;
				mesh.index_buffer = ib;
			}

		}

		return chunks;

	}

	std::vector<entity> EntityLoader::LoadObjMesh(std::string const& model_path)
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
		//std::vector<tinyobj::material_t> const& materials = reader.GetMaterials(); ignore materials for now

		// Loop over shapes
		std::vector<TexturedNormalVertex> vertices{};
		std::vector<uint32> indices{};
		std::vector<entity> entities{};

		for (size_t s = 0; s < shapes.size(); s++)
		{
			entity e = reg.create();
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
					indices.push_back(index_offset + v);

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

		std::shared_ptr<VertexBuffer> vb = std::make_shared<VertexBuffer>(gfx, vertices);
		std::shared_ptr<IndexBuffer> ib = std::make_shared<IndexBuffer>(gfx, indices);

		for (entity e : entities)
		{
			auto& mesh = reg.get<Mesh>(e);
			mesh.vertex_buffer = vb;
			mesh.index_buffer = ib;
			reg.emplace<Tag>(e, model_name + " mesh" + std::to_string(as_integer(e)));
		}

		ADRIA_LOG(INFO, "OBJ Mesh %s successfully loaded!", model_path.c_str());
		return entities;
	}

	EntityLoader::EntityLoader(registry& reg, GraphicsCoreDX12* gfx, TextureManager& texture_manager)
        : reg(reg), gfx(gfx), texture_manager(texture_manager)
    {
    }

	[[maybe_unused]]
	std::vector<entity> EntityLoader::LoadGLTFModel(model_parameters_t const& params)
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
		}
		if (!ret)
		{
			ADRIA_LOG(ERROR, "Failed to load model %s", model_name.c_str());
		}

		std::vector<CompleteVertex> vertices{};
		std::vector<uint32> indices{};
		std::vector<entity> entities{};

		tinygltf::Scene const& scene = model.scenes[model.defaultScene];
		for (size_t i = 0; i < scene.nodes.size(); ++i)
		{
			tinygltf::Node const& node = model.nodes[scene.nodes[i]]; //has children: model.nodes[node.children[i]] todo
			if (node.mesh < 0 || node.mesh >= model.meshes.size()) continue;

			tinygltf::Mesh const& node_mesh = model.meshes[node.mesh];
			for (size_t i = 0; i < node_mesh.primitives.size(); ++i)
			{
				tinygltf::Primitive primitive = node_mesh.primitives[i];
				tinygltf::Accessor const& index_accessor = model.accessors[primitive.indices];

				entity e = reg.create();
				entities.push_back(e);

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
				reg.emplace<Mesh>(e, mesh_component);

				tinygltf::Accessor const& position_accessor = model.accessors[primitive.attributes["POSITION"]];
				tinygltf::BufferView const& position_buffer_view = model.bufferViews[position_accessor.bufferView];
				tinygltf::Buffer const& position_buffer = model.buffers[position_buffer_view.buffer];
				int const position_byte_stride = position_accessor.ByteStride(position_buffer_view);
				uint8 const* positions = &position_buffer.data[position_buffer_view.byteOffset + position_accessor.byteOffset];

				tinygltf::Accessor const& texcoord_accessor = model.accessors[primitive.attributes["TEXCOORD_0"]];
				tinygltf::BufferView const& texcoord_buffer_view = model.bufferViews[texcoord_accessor.bufferView];
				tinygltf::Buffer const& texcoord_buffer = model.buffers[texcoord_buffer_view.buffer];
				int const texcoord_byte_stride = texcoord_accessor.ByteStride(texcoord_buffer_view);
				uint8 const* texcoords = &texcoord_buffer.data[texcoord_buffer_view.byteOffset + texcoord_accessor.byteOffset];

				tinygltf::Accessor const& normal_accessor = model.accessors[primitive.attributes["NORMAL"]];
				tinygltf::BufferView const& normal_buffer_view = model.bufferViews[normal_accessor.bufferView];
				tinygltf::Buffer const& normal_buffer = model.buffers[normal_buffer_view.buffer];
				int const normal_byte_stride = normal_accessor.ByteStride(normal_buffer_view);
				uint8 const* normals = &normal_buffer.data[normal_buffer_view.byteOffset + normal_accessor.byteOffset];

				tinygltf::Accessor const& tangent_accessor = model.accessors[primitive.attributes["TANGENT"]];
				tinygltf::BufferView const& tangent_buffer_view = model.bufferViews[tangent_accessor.bufferView];
				tinygltf::Buffer const& tangent_buffer = model.buffers[tangent_buffer_view.buffer];
				int const tangent_byte_stride = tangent_accessor.ByteStride(tangent_buffer_view);
				uint8 const* tangents = &tangent_buffer.data[tangent_buffer_view.byteOffset + tangent_accessor.byteOffset];

				ADRIA_ASSERT(position_accessor.count == texcoord_accessor.count);
				ADRIA_ASSERT(position_accessor.count == normal_accessor.count);

				std::vector<XMFLOAT3> position_array(position_accessor.count), normal_array(normal_accessor.count),
					tangent_array(position_accessor.count), bitangent_array(position_accessor.count);
				std::vector<XMFLOAT2> texcoord_array(texcoord_accessor.count);
				for (size_t i = 0; i < position_accessor.count; ++i)
				{
					XMFLOAT3 position;
					position.x = (reinterpret_cast<float32 const*>(positions + (i * position_byte_stride)))[0];
					position.y = (reinterpret_cast<float32 const*>(positions + (i * position_byte_stride)))[1];
					position.z = (reinterpret_cast<float32 const*>(positions + (i * position_byte_stride)))[2];
					position_array[i] = position;

					XMFLOAT2 texcoord;
					texcoord.x = (reinterpret_cast<float32 const*>(texcoords + (i * texcoord_byte_stride)))[0];
					texcoord.y = (reinterpret_cast<float32 const*>(texcoords + (i * texcoord_byte_stride)))[1];
					texcoord.y = 1.0f - texcoord.y;
					texcoord_array[i] = texcoord;

					XMFLOAT3 normal;
					normal.x = (reinterpret_cast<float32 const*>(normals + (i * normal_byte_stride)))[0];
					normal.y = (reinterpret_cast<float32 const*>(normals + (i * normal_byte_stride)))[1];
					normal.z = (reinterpret_cast<float32 const*>(normals + (i * normal_byte_stride)))[2];
					normal_array[i] = normal;

					XMFLOAT3 tangent{};
					XMFLOAT3 bitangent{};
					if (tangents)
					{
						tangent.x = (reinterpret_cast<float32 const*>(tangents + (i * tangent_byte_stride)))[0];
						tangent.y = (reinterpret_cast<float32 const*>(tangents + (i * tangent_byte_stride)))[1];
						tangent.z = (reinterpret_cast<float32 const*>(tangents + (i * tangent_byte_stride)))[2];
						float tangent_w = (reinterpret_cast<float32 const*>(tangents + (i * tangent_byte_stride)))[3];

						XMVECTOR _bitangent = XMVectorScale(XMVector3Cross(XMLoadFloat3(&normal), XMLoadFloat3(&tangent)), tangent_w);

						XMStoreFloat3(&bitangent, XMVector3Normalize(_bitangent));
					}

					tangent_array[i] = tangent;
					bitangent_array[i] = bitangent;
				}

				tinygltf::BufferView const& index_buffer_view = model.bufferViews[index_accessor.bufferView];
				tinygltf::Buffer const& index_buffer = model.buffers[index_buffer_view.buffer];
				int const index_byte_stride = index_accessor.ByteStride(index_buffer_view);
				uint8 const* indexes = index_buffer.data.data() + index_buffer_view.byteOffset + index_accessor.byteOffset;

				indices.reserve(indices.size() + index_accessor.count);
				for (size_t i = 0; i < index_accessor.count; ++i)
				{
					uint32 index = -1;
					switch (index_accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						index = (uint32)(reinterpret_cast<uint16 const*>(indexes + (i * index_byte_stride)))[0];
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						index = (reinterpret_cast<uint32 const*>(indexes + (i * index_byte_stride)))[0];
						break;
					default:
						ADRIA_ASSERT(false);
					}

					indices.push_back(index);
				}

				if (!tangents) //need to generate tangents and bitangents
				{
					ComputeTangentFrame(indices.data() - index_accessor.count, index_accessor.count,
						position_array.data(), normal_array.data(), texcoord_array.data(), position_accessor.count,
						tangent_array.data(), bitangent_array.data());
				}

				vertices.reserve(vertices.size() + position_accessor.count);
				for (size_t i = 0; i < position_accessor.count; ++i)
				{
					vertices.push_back(CompleteVertex{
							position_array[i],
							texcoord_array[i],
							normal_array[i],
							tangent_array[i],
							bitangent_array[i]
						});
				}


				Material material{};
				tinygltf::Material gltf_material = model.materials[primitive.material];
				tinygltf::PbrMetallicRoughness pbr_metallic_roughness = gltf_material.pbrMetallicRoughness;

				if (pbr_metallic_roughness.baseColorTexture.index >= 0)
				{
					tinygltf::Texture const& base_texture = model.textures[pbr_metallic_roughness.baseColorTexture.index];
					tinygltf::Image const& base_image = model.images[base_texture.source];
					std::string texbase = params.textures_path + base_image.uri;
					material.albedo_texture = texture_manager.LoadTexture(ConvertToWide(texbase));
					material.albedo_factor = (float32)pbr_metallic_roughness.baseColorFactor[0];
				}

				if (pbr_metallic_roughness.metallicRoughnessTexture.index >= 0)
				{
					tinygltf::Texture const& metallic_roughness_texture = model.textures[pbr_metallic_roughness.metallicRoughnessTexture.index];
					tinygltf::Image const& metallic_roughness_image = model.images[metallic_roughness_texture.source];
					std::string texmetallicroughness = params.textures_path + metallic_roughness_image.uri;
					material.metallic_roughness_texture = texture_manager.LoadTexture(ConvertToWide(texmetallicroughness));
					material.metallic_factor = (float32)pbr_metallic_roughness.metallicFactor;
					material.roughness_factor = (float32)pbr_metallic_roughness.roughnessFactor;
				}

				if (gltf_material.normalTexture.index >= 0)
				{
					tinygltf::Texture const& normal_texture = model.textures[gltf_material.normalTexture.index];
					tinygltf::Image const& normal_image = model.images[normal_texture.source];
					std::string texnormal = params.textures_path + normal_image.uri;
					material.normal_texture = texture_manager.LoadTexture(ConvertToWide(texnormal));
				}

				if (gltf_material.emissiveTexture.index >= 0)
				{
					tinygltf::Texture const& emissive_texture = model.textures[gltf_material.emissiveTexture.index];
					tinygltf::Image const& emissive_image = model.images[emissive_texture.source];
					std::string texemissive = params.textures_path + emissive_image.uri;
					material.emissive_texture = texture_manager.LoadTexture(ConvertToWide(texemissive));
					material.emissive_factor = (float32)gltf_material.emissiveFactor[0];
				}
				material.pso = EPipelineStateObject::GbufferPBR;

				reg.emplace<Material>(e, material);

				XMMATRIX model = params.model_matrix;
				BoundingBox aabb = AABBFromRange(vertices.end() - position_accessor.count, vertices.end());
				aabb.Transform(aabb, model);

				reg.emplace<Visibility>(e, aabb, true, true);
				reg.emplace<Transform>(e, model, model);
				reg.emplace<Deferred>(e);
			}
		}

		std::shared_ptr<VertexBuffer> vb = std::make_shared<VertexBuffer>(gfx, vertices);
		std::shared_ptr<IndexBuffer> ib = std::make_shared<IndexBuffer>(gfx, indices);

		for (entity e : entities)
		{
			auto& mesh = reg.get<Mesh>(e);
			mesh.vertex_buffer = vb;
			mesh.index_buffer = ib;
			reg.emplace<Tag>(e, model_name + " mesh" + std::to_string(as_integer(e)));
		}
		ADRIA_LOG(INFO, "GLTF Mesh %s successfully loaded!", params.model_path.c_str());
		return entities;
	}

	[[maybe_unused]]
	entity EntityLoader::LoadSkybox(skybox_parameters_t const& params)
    {
        entity skybox = reg.create();

        Skybox sky{};
        sky.active = true;

        if (params.cubemap.has_value()) sky.cubemap_texture = texture_manager.LoadCubemap(params.cubemap.value());
        else sky.cubemap_texture = texture_manager.LoadCubemap(params.cubemap_textures);

        reg.emplace<Skybox>(skybox, sky);

        reg.emplace<Tag>(skybox, "Skybox");

        return skybox;

    }

    [[maybe_unused]] 
    entity EntityLoader::LoadLight(light_parameters_t const& params)
    {
        entity light = reg.create();

        if (params.light_data.type == ELightType::Directional)
            const_cast<light_parameters_t&>(params).light_data.position = XMVectorScale(-params.light_data.direction, 1e3);

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

            Mesh mesh{};
            mesh.vertex_buffer = std::make_shared<VertexBuffer>(gfx, vertices);
            mesh.index_buffer = std::make_shared<IndexBuffer>(gfx, indices);
            mesh.indices_count = static_cast<uint32>(indices.size());

            reg.emplace<Mesh>(light, mesh);

            Material material{};
            XMStoreFloat3(&material.diffuse, params.light_data.color);

            if (params.light_texture.has_value())
                material.albedo_texture = texture_manager.LoadTexture(params.light_texture.value()); //
            else if (params.light_data.type == ELightType::Directional)
                material.albedo_texture = texture_manager.LoadTexture(L"Resources/Textures/sun.png");

            if (params.light_data.type == ELightType::Directional)
                material.pso = EPipelineStateObject::Sun;
            else if (material.albedo_texture != INVALID_TEXTURE_HANDLE)
                material.pso = EPipelineStateObject::Billboard;
			else 
			{ 
				ADRIA_LOG(ERROR, "Light with quad mesh needs diffuse texture!"); 
			}

            reg.emplace<Material>(light, material);

            BoundingBox aabb = AABBFromRange(vertices.begin(), vertices.end());
            auto translation_matrix = XMMatrixTranslationFromVector(params.light_data.position);
            aabb.Transform(aabb, XMMatrixTranslationFromVector(params.light_data.position));

            reg.emplace<Visibility>(light, aabb, true, true);
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

	std::vector<entity> EntityLoader::LoadOcean(ocean_parameters_t const& params)
	{
		std::vector<entity> ocean_chunks = EntityLoader::LoadGrid(params.ocean_grid);

		Material ocean_material{};
		ocean_material.diffuse = XMFLOAT3(0.0123f, 0.3613f, 0.6867f);
		ocean_material.pso = EPipelineStateObject::Unknown;
		Ocean ocean_component{};

		for (auto ocean_chunk : ocean_chunks)
		{
			reg.emplace<Material>(ocean_chunk, ocean_material);
			reg.emplace<Ocean>(ocean_chunk, ocean_component);
			reg.emplace<Tag>(ocean_chunk, "Ocean Chunk" + std::to_string(as_integer(ocean_chunk)));
		}

		return ocean_chunks;
	}

	[[maybe_unused]]
	entity EntityLoader::LoadEmitter(emitter_parameters_t const& params)
	{
		Emitter emitter{};
		emitter.position = DirectX::XMFLOAT4(params.position[0], params.position[1], params.position[2], 1);
		emitter.velocity = DirectX::XMFLOAT4(params.velocity[0], params.velocity[1], params.velocity[2], 0);
		emitter.position_variance = DirectX::XMFLOAT4(params.position_variance[0], params.position_variance[1], params.position_variance[2], 1);
		emitter.velocity_variance = params.velocity_variance;
		emitter.number_to_emit = 0;
		emitter.particle_lifespan = params.lifespan;
		emitter.start_size = params.start_size;
		emitter.end_size = params.end_size;
		emitter.mass = params.mass;
		emitter.particles_per_second = params.particles_per_second;
		emitter.sort = params.sort;
		emitter.collisions_enabled = params.collisions;
		emitter.particle_texture = texture_manager.LoadTexture(params.texture_path);

		tecs::entity emitter_entity = reg.create();
		reg.add(emitter_entity, emitter);

		if (params.name.empty()) reg.emplace<Tag>(emitter_entity);
		else reg.emplace<Tag>(emitter_entity, params.name);

		return emitter_entity;
	}

	[[maybe_unused]]
	entity EntityLoader::LoadDecal(decal_parameters_t const& params)
	{
		Decal decal{};
		if (!params.albedo_texture_path.empty()) decal.albedo_decal_texture = texture_manager.LoadTexture(ConvertToWide(params.albedo_texture_path));
		if (!params.normal_texture_path.empty()) decal.normal_decal_texture = texture_manager.LoadTexture(ConvertToWide(params.normal_texture_path));

		XMVECTOR P = XMLoadFloat4(&params.position);
		XMVECTOR N = XMLoadFloat4(&params.normal);

		XMVECTOR ProjectorDirection = XMVectorNegate(N);
		XMMATRIX RotationMatrix = XMMatrixRotationAxis(ProjectorDirection, params.rotation);
		XMMATRIX ModelMatrix = XMMatrixScaling(params.size, params.size, params.size) * RotationMatrix * XMMatrixTranslationFromVector(P);

		decal.decal_model_matrix = ModelMatrix;
		decal.decal_type = params.decal_type;
		decal.modify_gbuffer_normals = params.modify_gbuffer_normals;

		entity decal_entity = reg.create();
		reg.add(decal_entity, decal);
		if (params.name.empty()) reg.emplace<Tag>(decal_entity, "decal");
		else reg.emplace<Tag>(decal_entity, params.name);

		return decal_entity;
	}

}