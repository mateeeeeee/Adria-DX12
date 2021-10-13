#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include "tiny_gltf.h"

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
		if (!warn.empty()) Log::Warning(warn.c_str());
		if (!err.empty()) Log::Error(err.c_str());
		if (!ret) Log::Error("Failed to load model " + model_name);

		std::vector<CompleteVertex> vertices{};
		std::vector<u32> indices{};
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
				mesh_component.indices_count = static_cast<u32>(index_accessor.count);
				mesh_component.start_index_location = static_cast<u32>(indices.size());
				mesh_component.vertex_offset = static_cast<u32>(vertices.size());
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
				u8 const* positions = &position_buffer.data[position_buffer_view.byteOffset + position_accessor.byteOffset];

				tinygltf::Accessor const& texcoord_accessor = model.accessors[primitive.attributes["TEXCOORD_0"]];
				tinygltf::BufferView const& texcoord_buffer_view = model.bufferViews[texcoord_accessor.bufferView];
				tinygltf::Buffer const& texcoord_buffer = model.buffers[texcoord_buffer_view.buffer];
				int const texcoord_byte_stride = texcoord_accessor.ByteStride(texcoord_buffer_view);
				u8 const* texcoords = &texcoord_buffer.data[texcoord_buffer_view.byteOffset + texcoord_accessor.byteOffset];

				tinygltf::Accessor const& normal_accessor = model.accessors[primitive.attributes["NORMAL"]];
				tinygltf::BufferView const& normal_buffer_view = model.bufferViews[normal_accessor.bufferView];
				tinygltf::Buffer const& normal_buffer = model.buffers[normal_buffer_view.buffer];
				int const normal_byte_stride = normal_accessor.ByteStride(normal_buffer_view);
				u8 const* normals = &normal_buffer.data[normal_buffer_view.byteOffset + normal_accessor.byteOffset];

				tinygltf::Accessor const& tangent_accessor = model.accessors[primitive.attributes["TANGENT"]];
				tinygltf::BufferView const& tangent_buffer_view = model.bufferViews[tangent_accessor.bufferView];
				tinygltf::Buffer const& tangent_buffer = model.buffers[tangent_buffer_view.buffer];
				int const tangent_byte_stride = tangent_accessor.ByteStride(tangent_buffer_view);
				u8 const* tangents = &tangent_buffer.data[tangent_buffer_view.byteOffset + tangent_accessor.byteOffset];

				ADRIA_ASSERT(position_accessor.count == texcoord_accessor.count);
				ADRIA_ASSERT(position_accessor.count == normal_accessor.count);

				std::vector<XMFLOAT3> position_array(position_accessor.count), normal_array(normal_accessor.count),
					tangent_array(position_accessor.count), bitangent_array(position_accessor.count);
				std::vector<XMFLOAT2> texcoord_array(texcoord_accessor.count);
				for (size_t i = 0; i < position_accessor.count; ++i)
				{
					XMFLOAT3 position;
					position.x = (reinterpret_cast<f32 const*>(positions + (i * position_byte_stride)))[0];
					position.y = (reinterpret_cast<f32 const*>(positions + (i * position_byte_stride)))[1];
					position.z = (reinterpret_cast<f32 const*>(positions + (i * position_byte_stride)))[2];
					position_array[i] = position;

					XMFLOAT2 texcoord;
					texcoord.x = (reinterpret_cast<f32 const*>(texcoords + (i * texcoord_byte_stride)))[0];
					texcoord.y = (reinterpret_cast<f32 const*>(texcoords + (i * texcoord_byte_stride)))[1];
					texcoord.y = 1.0f - texcoord.y;
					texcoord_array[i] = texcoord;

					XMFLOAT3 normal;
					normal.x = (reinterpret_cast<f32 const*>(normals + (i * normal_byte_stride)))[0];
					normal.y = (reinterpret_cast<f32 const*>(normals + (i * normal_byte_stride)))[1];
					normal.z = (reinterpret_cast<f32 const*>(normals + (i * normal_byte_stride)))[2];
					normal_array[i] = normal;

					XMFLOAT3 tangent{};
					XMFLOAT3 bitangent{};
					if (tangents)
					{
						tangent.x = (reinterpret_cast<f32 const*>(tangents + (i * tangent_byte_stride)))[0];
						tangent.y = (reinterpret_cast<f32 const*>(tangents + (i * tangent_byte_stride)))[1];
						tangent.z = (reinterpret_cast<f32 const*>(tangents + (i * tangent_byte_stride)))[2];
						float tangent_w = (reinterpret_cast<f32 const*>(tangents + (i * tangent_byte_stride)))[3];

						XMVECTOR _bitangent = XMVectorScale(XMVector3Cross(XMLoadFloat3(&normal), XMLoadFloat3(&tangent)), tangent_w);

						XMStoreFloat3(&bitangent, XMVector3Normalize(_bitangent));
					}

					tangent_array[i] = tangent;
					bitangent_array[i] = bitangent;
				}

				tinygltf::BufferView const& index_buffer_view = model.bufferViews[index_accessor.bufferView];
				tinygltf::Buffer const& index_buffer = model.buffers[index_buffer_view.buffer];
				int const index_byte_stride = index_accessor.ByteStride(index_buffer_view);
				u8 const* indexes = index_buffer.data.data() + index_buffer_view.byteOffset + index_accessor.byteOffset;

				indices.reserve(indices.size() + index_accessor.count);
				for (size_t i = 0; i < index_accessor.count; ++i)
				{
					u32 index = -1;
					switch (index_accessor.componentType)
					{
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						index = (u32)(reinterpret_cast<u16 const*>(indexes + (i * index_byte_stride)))[0];
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						index = (reinterpret_cast<u32 const*>(indexes + (i * index_byte_stride)))[0];
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
					material.albedo_factor = (f32)pbr_metallic_roughness.baseColorFactor[0];
				}

				if (pbr_metallic_roughness.metallicRoughnessTexture.index >= 0)
				{
					tinygltf::Texture const& metallic_roughness_texture = model.textures[pbr_metallic_roughness.metallicRoughnessTexture.index];
					tinygltf::Image const& metallic_roughness_image = model.images[metallic_roughness_texture.source];
					std::string texmetallicroughness = params.textures_path + metallic_roughness_image.uri;
					material.metallic_roughness_texture = texture_manager.LoadTexture(ConvertToWide(texmetallicroughness));
					material.metallic_factor = (f32)pbr_metallic_roughness.metallicFactor;
					material.roughness_factor = (f32)pbr_metallic_roughness.roughnessFactor;
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
					material.emissive_factor = (f32)gltf_material.emissiveFactor[0];
				}
				material.pso = EPipelineStateObject::GbufferPBR;

				reg.emplace<Material>(e, material);

				XMMATRIX model = XMMatrixScaling(params.model_scale, params.model_scale, params.model_scale);
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
			mesh.vb = vb;
			mesh.ib = ib;
			reg.emplace<Tag>(e, model_name + " mesh" + std::to_string(as_integer(e)));
		}

		Log::Info("Model" + params.model_path + " successfully loaded!");

		return entities;
	}

	[[maybe_unused]]
	entity EntityLoader::LoadSkybox(skybox_parameters_t const& params)
    {
        entity skybox = reg.create();

        static const SimpleVertex cube_vertices[8] = {
            XMFLOAT3{ -1.0, -1.0,  1.0 },
            XMFLOAT3{ 1.0, -1.0,  1.0 },
            XMFLOAT3{ 1.0,  1.0,  1.0 },
            XMFLOAT3{ -1.0,  1.0,  1.0 },
            XMFLOAT3{ -1.0, -1.0, -1.0 },
            XMFLOAT3{ 1.0, -1.0, -1.0 },
            XMFLOAT3{ 1.0,  1.0, -1.0 },
            XMFLOAT3{ -1.0,  1.0, -1.0 }
        };

        static const uint16_t cube_indices[36] = {
            // front
            0, 1, 2,
            2, 3, 0,
            // right
            1, 5, 6,
            6, 2, 1,
            // back
            7, 6, 5,
            5, 4, 7,
            // left
            4, 0, 3,
            3, 7, 4,
            // bottom
            4, 5, 1,
            1, 0, 4,
            // top
            3, 2, 6,
            6, 7, 3
        };

        Mesh skybox_mesh{};
        skybox_mesh.vb = std::make_shared<VertexBuffer>(gfx, cube_vertices, _countof(cube_vertices));
        skybox_mesh.ib = std::make_shared<IndexBuffer>(gfx, cube_indices, _countof(cube_indices));
        skybox_mesh.indices_count = _countof(cube_indices);
        reg.emplace<Mesh>(skybox, skybox_mesh);

        reg.emplace<Transform>(skybox, Transform{});

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
            u32 const size = params.mesh_size;
            std::vector<TexturedVertex> const vertices =
            {
                { {-0.5f * size, -0.5f * size, 0.0f}, {0.0f, 0.0f}},
                { { 0.5f * size, -0.5f * size, 0.0f}, {1.0f, 0.0f}},
                { { 0.5f * size,  0.5f * size, 0.0f}, {1.0f, 1.0f}},
                { {-0.5f * size,  0.5f * size, 0.0f}, {0.0f, 1.0f}}
            };

            std::vector<u16> const indices =
            {
                    0, 2, 1, 2, 0, 3
            };

            Mesh mesh{};
            mesh.vb = std::make_shared<VertexBuffer>(gfx, vertices);
            mesh.ib = std::make_shared<IndexBuffer>(gfx, indices);
            mesh.indices_count = static_cast<u32>(indices.size());

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
            else GLOBAL_LOG_ERROR("Light with quad mesh needs diffuse texture!");

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
}