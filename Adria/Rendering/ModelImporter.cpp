
#include "assimp/Importer.hpp"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/pbrmaterial.h"

#include "ModelImporter.h"
#include "Components.h"
#include "../Graphics/GraphicsCoreDX12.h"
#include "../Logging/Logger.h"
#include "../Math/BoundingVolumeHelpers.h"
#include "../Core/Definitions.h"
#include "../Utilities/StringUtil.h"
#include "../Utilities/FilesUtil.h"

using namespace DirectX;


namespace adria
{
    using namespace tecs;


    ModelImporter::ModelImporter(registry& reg, GraphicsCoreDX12* gfx, TextureManager& texture_manager)
        : reg(reg), gfx(gfx), texture_manager(texture_manager)
    {
    }

    entity ModelImporter::LoadSkybox(skybox_parameters_t const& params)
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

    std::vector<entity> ModelImporter::LoadGLTFModel(model_parameters_t const& params)
    {
        Assimp::Importer importer;

        const auto importer_flags =
            aiProcess_CalcTangentSpace |
            aiProcess_GenSmoothNormals |
            aiProcess_JoinIdenticalVertices |
            aiProcess_OptimizeMeshes |
            aiProcess_ImproveCacheLocality |
            aiProcess_RemoveRedundantMaterials |
            aiProcess_LimitBoneWeights |
            aiProcess_SplitLargeMeshes |
            aiProcess_Triangulate |
            aiProcess_GenUVCoords |
            aiProcess_SortByPType |
            aiProcess_FindDegenerates |
            aiProcess_FindInvalidData |
            aiProcess_FindInstances |
            aiProcess_ValidateDataStructure |
            aiProcess_Debone;

        aiScene const* scene = importer.ReadFile(params.model_path, importer_flags);
        if (scene == nullptr)
        {
            GLOBAL_LOG_ERROR("Assimp Model Loading Failed for file " + params.model_path + "!");
            return {};
        }
        auto model_name = GetFilename(params.model_path);


        std::vector<CompleteVertex> vertices{};
        std::vector<u32> indices{};
        std::vector<entity> entities{};
        for (u32 mesh_index = 0; mesh_index < scene->mNumMeshes; mesh_index++)
        {
            entity e = reg.create();
            entities.push_back(e);

            const aiMesh* mesh = scene->mMeshes[mesh_index];

            Mesh mesh_component{};
            mesh_component.indices_count = static_cast<u32>(mesh->mNumFaces * 3);
            mesh_component.start_index_location = static_cast<u32>(indices.size());
            mesh_component.vertex_offset = static_cast<u32>(vertices.size());

            reg.emplace<Mesh>(e, mesh_component);

            vertices.reserve(vertices.size() + mesh->mNumVertices);

            for (u32 i = 0; i < mesh->mNumVertices; ++i)
            {
                vertices.push_back(
                    {
                        reinterpret_cast<XMFLOAT3&>(mesh->mVertices[i]),
                        reinterpret_cast<XMFLOAT2&>(mesh->mTextureCoords[0][i]),
                        reinterpret_cast<XMFLOAT3&>(mesh->mNormals[i]),
                        reinterpret_cast<XMFLOAT3&>(mesh->mTangents[i]),
                        reinterpret_cast<XMFLOAT3&>(mesh->mBitangents[i])
                    }
                );
            }


            indices.reserve(indices.size() + mesh->mNumFaces * 3);

            for (u32 i = 0; i < mesh->mNumFaces; ++i)
            {
                indices.push_back(mesh->mFaces[i].mIndices[0]);
                indices.push_back(mesh->mFaces[i].mIndices[1]);
                indices.push_back(mesh->mFaces[i].mIndices[2]);
            }

            const aiMaterial* srcMat = scene->mMaterials[mesh->mMaterialIndex];


            Material material{};

            float albedo_factor = 1.0f, metallic_factor = 1.0f, roughness_factor = 1.0f, emissive_factor = 1.0f;


            //if (srcMat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_BASE_COLOR_FACTOR, albedo_factor) == AI_SUCCESS)
            //{
            //    material.albedo_factor = albedo_factor;
            //}
            if (srcMat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_METALLIC_FACTOR, metallic_factor) == AI_SUCCESS)
            {
                material.metallic_factor = metallic_factor;
            }

            if (srcMat->Get(AI_MATKEY_GLTF_PBRMETALLICROUGHNESS_ROUGHNESS_FACTOR, roughness_factor) == AI_SUCCESS)
            {
                material.roughness_factor = roughness_factor;
            }
            //if (srcMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive_factor) == AI_SUCCESS)
            //{
            //    material.emissive_factor = emissive_factor;
            //}

            aiString texAlbedoPath;
            aiString texRoughnessMetallicPath;
            aiString texNormalPath;
            aiString texEmissivePath;

            auto debug = static_cast<u64>(e);


            if (AI_SUCCESS == srcMat->Get(AI_MATKEY_TEXTURE(aiTextureType_DIFFUSE, 1), texAlbedoPath))
            {
                std::string texalbedo = params.textures_path + texAlbedoPath.C_Str();

                material.albedo_texture = texture_manager.LoadTexture(ConvertToWide(texalbedo));
            }
            

            if (AI_SUCCESS == srcMat->Get(AI_MATKEY_TEXTURE(aiTextureType_UNKNOWN, 0), texRoughnessMetallicPath))
            {
                std::string texmetallicroughness = params.textures_path + texRoughnessMetallicPath.C_Str();

                material.metallic_roughness_texture = texture_manager.LoadTexture(ConvertToWide(texmetallicroughness));
            }
            

            if (AI_SUCCESS == srcMat->Get(AI_MATKEY_TEXTURE(aiTextureType_NORMALS, 0), texNormalPath))
            {

                std::string texnorm = params.textures_path + texNormalPath.C_Str();

                material.normal_texture = texture_manager.LoadTexture(ConvertToWide(texnorm));
            }

            if (AI_SUCCESS == srcMat->Get(AI_MATKEY_TEXTURE(aiTextureType_EMISSIVE, 0), texEmissivePath))
            {
                std::string texemissive = params.textures_path + texEmissivePath.C_Str();

                material.emissive_texture = texture_manager.LoadTexture(ConvertToWide(texemissive));

            }

            material.pso = PSO::eGbufferPBR;

            reg.add<Material>(e, material);

            BoundingBox aabb = AABBFromRange(vertices.end() - mesh->mNumVertices, vertices.end());
            aabb.Transform(aabb, params.model);

            reg.emplace<Visibility>(e, aabb, true, true);
            reg.emplace<Transform>(e, params.model, params.model);
            reg.emplace<Deferred>(e);
        }


        std::shared_ptr<VertexBuffer> vb = std::make_shared<VertexBuffer>(gfx, vertices);
        std::shared_ptr<IndexBuffer> ib = std::make_shared<IndexBuffer>(gfx, indices);

        for (entity e : entities)
        {
            auto& mesh = reg.get<Mesh>(e);
            auto& mat = reg.get<Material>(e);
            mesh.vb = vb;
            mesh.ib = ib;
            reg.emplace<Tag>(e, model_name + " mesh" + std::to_string(as_integer(e)));
        }

        Log::Info("Model" + params.model_path + " successfully loaded!");


        return entities;

    }

    [[maybe_unused]] 
    entity ModelImporter::LoadLight(light_parameters_t const& params)
    {
        entity light = reg.create();

        if (params.light_data.type == LightType::eDirectional)
            const_cast<light_parameters_t&>(params).light_data.position = XMVectorScale(-params.light_data.direction, 1e3);


        reg.emplace<Light>(light, params.light_data);

        if (params.mesh_type == LightMesh::eQuad)
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
                material.diffuse_texture = texture_manager.LoadTexture(params.light_texture.value()); //
            else if (params.light_data.type == LightType::eDirectional)
                material.diffuse_texture = texture_manager.LoadTexture(L"Resources/Textures/sun.png");

            if (params.light_data.type == LightType::eDirectional)
                material.pso = PSO::eSun;
            else if (material.diffuse_texture != INVALID_TEXTURE_HANDLE)
                material.pso = PSO::eBillboard;
            else GLOBAL_LOG_ERROR("Light with quad mesh needs diffuse texture!");

            reg.emplace<Material>(light, material);

            BoundingBox aabb = AABBFromRange(vertices.begin(), vertices.end());
            auto translation_matrix = XMMatrixTranslationFromVector(params.light_data.position);
            aabb.Transform(aabb, XMMatrixTranslationFromVector(params.light_data.position));

            reg.emplace<Visibility>(light, aabb, true, true);
            reg.emplace<Transform>(light, translation_matrix, translation_matrix);
            reg.emplace<Forward>(light, true);
        }
        else if (params.mesh_type == LightMesh::eSphere)
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
        case LightType::eDirectional:
            reg.emplace<Tag>(light, "Directional Light");
            break;
        case LightType::eSpot:
            reg.emplace<Tag>(light, "Spot Light");
            break;
        case LightType::ePoint:
            reg.emplace<Tag>(light, "Point Light");
            break;
        }


        return light;

    }

    void ModelImporter::LoadModelMesh(tecs::entity e, model_parameters_t const& params)
    {

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(params.model_path,
            aiProcess_CalcTangentSpace |
            aiProcess_JoinIdenticalVertices |
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_SplitLargeMeshes |
            aiProcess_RemoveRedundantMaterials |
            aiProcess_GenUVCoords |
            aiProcess_TransformUVCoords |
            aiProcess_FlipUVs |
            aiProcess_OptimizeMeshes |
            aiProcess_OptimizeGraph
        );

        if (scene == nullptr)
        {
            Log::Error(std::string("Unsuccessful Import: ") + importer.GetErrorString() + "\n");
            return;
        }


        std::vector<CompleteVertex> vertices;
        std::vector<u32> indices;

        if (scene->mNumMeshes > 1) Log::Warning("LoadModelMesh excepts only one mesh but there are more than one! \n");


        for (u32 meshIndex = 0; meshIndex < scene->mNumMeshes; meshIndex++)
        {

            const aiMesh* mesh = scene->mMeshes[meshIndex];

            Mesh mesh_component{};
            mesh_component.indices_count = static_cast<u32>(mesh->mNumFaces * 3);
            mesh_component.start_index_location = static_cast<u32>(indices.size());
            mesh_component.vertex_offset = static_cast<u32>(vertices.size());

            reg.emplace<Mesh>(e, mesh_component);

            vertices.reserve(vertices.size() + mesh->mNumVertices);



            for (u32 i = 0; i < mesh->mNumVertices; ++i)
            {
                vertices.push_back(
                    {
                        reinterpret_cast<DirectX::XMFLOAT3&>(mesh->mVertices[i]),
                        mesh->HasTextureCoords(0) ? reinterpret_cast<DirectX::XMFLOAT2 const&>(mesh->mTextureCoords[0][i]) : DirectX::XMFLOAT2{},
                        reinterpret_cast<DirectX::XMFLOAT3&>(mesh->mNormals[i]),
                        mesh->HasTangentsAndBitangents() ? reinterpret_cast<XMFLOAT3 const&>(mesh->mTangents[i]) : DirectX::XMFLOAT3{},
                        mesh->HasTangentsAndBitangents() ? reinterpret_cast<XMFLOAT3 const&>(mesh->mBitangents[i]) : DirectX::XMFLOAT3{},
                    }
                );
            }

            indices.reserve(indices.size() + mesh->mNumFaces * 3);

            for (u32 i = 0; i < mesh->mNumFaces; ++i)
            {
                indices.push_back(mesh->mFaces[i].mIndices[0]);
                indices.push_back(mesh->mFaces[i].mIndices[1]);
                indices.push_back(mesh->mFaces[i].mIndices[2]);
            }

        }

        std::shared_ptr<VertexBuffer> vb = std::make_shared<VertexBuffer>(gfx, vertices);
        std::shared_ptr<IndexBuffer> ib = std::make_shared<IndexBuffer>(gfx, indices);
        
        auto& mesh = reg.get<Mesh>(e);

        mesh.vb = vb;
        mesh.ib = ib;
        mesh.vertices = vertices;

        auto model_name = GetFilename(params.model_path);

        std::string name = model_name + std::to_string(as_integer(e));

        auto tag = reg.get_if<Tag>(e);
        if (tag)
            tag->name = name;
        else reg.emplace<Tag>(e, name);


    }
}