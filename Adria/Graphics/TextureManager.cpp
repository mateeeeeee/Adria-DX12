// Maybe use DirectXTex for mipmaps?
#include "DirectXTex.h"
#ifdef _DEBUG
#pragma comment(lib, "Debug\\DirectXTex.lib")
#else
#pragma comment(lib, "Release\\DirectXTex.lib")
#endif

#include "TextureManager.h"
#include "GraphicsCoreDX12.h"
#include "d3dx12.h"
#include "DDSTextureLoader12.h"
#include "WICTextureLoader12.h"
#include "ShaderUtility.h"
#include "../Utilities/StringUtil.h"
#include "../Utilities/Image.h"
#include "../Core/Macros.h"
#include "../Utilities/FilesUtil.h"

namespace adria
{

	namespace
	{
        enum class TextureFormat
        {
            eDDS,
            eBMP,
            eJPG,
            ePNG,
            eTIFF,
            eGIF,
            eICO,
            eTGA,
            eHDR,
            ePIC,
            eNotSupported
        };

        TextureFormat GetTextureFormat(std::string const& path)
        {
            std::string extension = GetExtension(path);
            std::transform(std::begin(extension), std::end(extension), std::begin(extension), [](char c) {return std::tolower(c); });

            if (extension == ".dds")
                return TextureFormat::eDDS;
            else if (extension == ".bmp")
                return TextureFormat::eBMP;
            else if (extension == ".jpg" || extension == ".jpeg")
                return TextureFormat::eJPG;
            else if (extension == ".png")
                return TextureFormat::ePNG;
            else if (extension == ".tiff" || extension == ".tif")
                return TextureFormat::eTIFF;
            else if (extension == ".gif")
                return TextureFormat::eGIF;
            else if (extension == ".ico")
                return TextureFormat::eICO;
            else if (extension == ".tga")
                return TextureFormat::eTGA;
            else if (extension == ".hdr")
                return TextureFormat::eHDR;
            else if (extension == ".pic")
                return TextureFormat::ePIC;
            else
                return TextureFormat::eNotSupported;
        }

        TextureFormat GetTextureFormat(std::wstring const& path)
        {
            return GetTextureFormat(ConvertToNarrow(path));
        }

        void CreateTextureSRV(ID3D12Device* device, ID3D12Resource* tex, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle)
        {
            D3D12_RESOURCE_DESC desc = tex->GetDesc();
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};

            if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
            {

                if (desc.DepthOrArraySize == 6)
                {
                    srv_desc.TextureCube.MostDetailedMip = 0;
                    srv_desc.TextureCube.MipLevels = -1;
                    srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;
                }
                else
                {
                    srv_desc.Texture2D.MostDetailedMip = 0;
                    srv_desc.Texture2D.MipLevels = -1;
                    srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
                }

                srv_desc.Format = desc.Format;
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.ViewDimension = desc.DepthOrArraySize == 6 ? D3D12_SRV_DIMENSION_TEXTURECUBE : D3D12_SRV_DIMENSION_TEXTURE2D;
            }
            else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
            {
                srv_desc.Format = desc.Format;
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                srv_desc.Texture3D.MipLevels = -1;
                srv_desc.Texture3D.MostDetailedMip = 0;
                srv_desc.Texture3D.ResourceMinLODClamp = 0.0f;
            }

            device->CreateShaderResourceView(tex, &srv_desc, cpu_handle);

        }

        void CreateNullSRV(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle) //bool is_cube
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.Texture2D.MostDetailedMip = 0;
            srv_desc.Texture2D.MipLevels = -1;
            srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;

            srv_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

            device->CreateShaderResourceView(nullptr, &srv_desc, cpu_handle);
        }
	}

    TextureManager::TextureManager(GraphicsCoreDX12* gfx, UINT max_textures) : gfx(gfx)
    {
        D3D12_DESCRIPTOR_HEAP_DESC texture_heap_desc = {};
        texture_heap_desc.NumDescriptors = max_textures;
        texture_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        texture_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; 
        texture_srv_heap.reset(new DescriptorHeap(gfx->GetDevice(), texture_heap_desc));
        mips_generator = std::make_unique<MipsGenerator>(gfx->GetDevice(), max_textures);

        CreateNullSRV(gfx->GetDevice(), texture_srv_heap->GetFirstHandle());
        {
            CD3DX12_DESCRIPTOR_RANGE1 const descriptor_ranges[] =
            {
                {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC},
                {D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE},
            };
            CD3DX12_ROOT_PARAMETER1 root_parameters[2] = {};
            root_parameters[0].InitAsDescriptorTable(1, &descriptor_ranges[0]);
            root_parameters[1].InitAsDescriptorTable(1, &descriptor_ranges[1]);
            CD3DX12_STATIC_SAMPLER_DESC sampler_desc{ 0, D3D12_FILTER_MIN_MAG_MIP_LINEAR };

            CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC signature_desc;
            signature_desc.Init_1_1(2, root_parameters, 1, &sampler_desc);
            Microsoft::WRL::ComPtr<ID3DBlob> signature;
            Microsoft::WRL::ComPtr<ID3DBlob> error;
            HRESULT hr = D3DX12SerializeVersionedRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);
            if (error)
            { 
                ADRIA_LOG(ERROR, (char const*)error->GetBufferPointer());
            }
            BREAK_IF_FAILED(gfx->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&equirect_root_signature)));
        }
        
        
        {
            ShaderBlob equirect_cs_shader;
            ShaderUtility::GetBlobFromCompiledShader(L"Resources/Compiled Shaders/Equirect2cubeCS.cso", equirect_cs_shader);

            D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc = {};
            pso_desc.pRootSignature = equirect_root_signature.Get();
            pso_desc.CS = equirect_cs_shader;
            BREAK_IF_FAILED(gfx->GetDevice()->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&equirect_pso)));
        }

        
    }

    [[nodiscard]]
    TEXTURE_HANDLE TextureManager::LoadTexture(std::wstring const& name)
    {
        TextureFormat format = GetTextureFormat(name);

        switch (format)
        {
        case TextureFormat::eDDS:
            return LoadDDSTexture(name);
        case TextureFormat::eBMP:
        case TextureFormat::ePNG:
        case TextureFormat::eJPG:
        case TextureFormat::eTIFF:
        case TextureFormat::eGIF:
        case TextureFormat::eICO:
            return LoadWICTexture(name);
        case TextureFormat::eTGA:
        case TextureFormat::eHDR:
        case TextureFormat::ePIC:
            return LoadTexture_HDR_TGA_PIC(ConvertToNarrow(name));
        case TextureFormat::eNotSupported:
        default:
            ADRIA_ASSERT(false && "Unsupported Texture Format!");
        }

       
        return INVALID_TEXTURE_HANDLE;
    }

    [[nodiscard]]
    TEXTURE_HANDLE TextureManager::LoadCubemap(std::wstring const& name)
    {
        TextureFormat format = GetTextureFormat(name);
        ADRIA_ASSERT(format == TextureFormat::eDDS || format == TextureFormat::eHDR && "Cubemap in one file has to be .dds or .hdr format");

        if (auto it = loaded_textures.find(name); it == loaded_textures.end())
        {
            ++handle;

            auto device = gfx->GetDevice();
            auto allocator = gfx->GetAllocator();
            auto cmd_list = gfx->GetDefaultCommandList();
            

            if (format == TextureFormat::eDDS)
            {
                ADRIA_ASSERT(texture_srv_heap->Count() > handle && "Not enough space for descriptors in Texture Cache");
                loaded_textures.insert({ name, handle });
                ID3D12Resource* cubemap = nullptr;
                std::unique_ptr<uint8_t[]> decodedData;

                std::vector<D3D12_SUBRESOURCE_DATA> subresources;

                bool is_cubemap;
                BREAK_IF_FAILED(
                    DirectX::LoadDDSTextureFromFile(device, name.c_str(), &cubemap,
                        decodedData, subresources, 0, nullptr, &is_cubemap));

                ADRIA_ASSERT(is_cubemap);
                const UINT64 uploadBufferSize = GetRequiredIntermediateSize(cubemap, 0,
					static_cast<UINT>(subresources.size()));
                D3D12MA::ALLOCATION_DESC texture_upload_alloc_desc{};
                texture_upload_alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
                D3D12_RESOURCE_DESC texture_upload_resource_desc{};
                texture_upload_resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                texture_upload_resource_desc.Alignment = 0;
                texture_upload_resource_desc.Width = uploadBufferSize;
                texture_upload_resource_desc.Height = 1;
                texture_upload_resource_desc.DepthOrArraySize = 1;
                texture_upload_resource_desc.MipLevels = 1;
                texture_upload_resource_desc.Format = DXGI_FORMAT_UNKNOWN;
                texture_upload_resource_desc.SampleDesc.Count = 1;
                texture_upload_resource_desc.SampleDesc.Quality = 0;
                texture_upload_resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                texture_upload_resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

                D3D12MA::Allocation* texture_upload_allocation;
                BREAK_IF_FAILED(allocator->CreateResource(
                    &texture_upload_alloc_desc,
                    &texture_upload_resource_desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    &texture_upload_allocation, __uuidof(nullptr), nullptr
                ));

                UpdateSubresources(cmd_list, cubemap, texture_upload_allocation->GetResource(),
                    0, 0, static_cast<UINT>(subresources.size()), subresources.data());
                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(cubemap,
                    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                cmd_list->ResourceBarrier(1, &barrier);
                gfx->AddToReleaseQueue(texture_upload_allocation);
                texture_map.insert({ handle, cubemap });
                CreateTextureSRV(device, texture_map[handle].Get(), texture_srv_heap->GetHandle(handle));
            }
            else //format == TextureFormat::eHDR
            {
                auto descriptor_allocator = gfx->GetDescriptorAllocator();
                loaded_textures.insert({ name, handle });
                Image equirect_hdr_image(ConvertToNarrow(name));
                Microsoft::WRL::ComPtr<ID3D12Resource> cubemap_tex = nullptr;
                D3D12_RESOURCE_DESC desc{};
                desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                desc.Width = 1024;
                desc.Height = 1024;
                desc.DepthOrArraySize = 6;
                desc.MipLevels = 1;
                desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
                desc.SampleDesc.Count = 1;
                desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

                auto heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };
                BREAK_IF_FAILED(device->CreateCommittedResource(
                    &heap_properties,
                    D3D12_HEAP_FLAG_NONE,
                    &desc,
                    D3D12_RESOURCE_STATE_COMMON,
                    nullptr,
                    IID_PPV_ARGS(&cubemap_tex)));

                D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                srv_desc.Format = desc.Format;
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.TextureCube.MipLevels = -1;
                srv_desc.TextureCube.MostDetailedMip = 0;
                device->CreateShaderResourceView(cubemap_tex.Get(), &srv_desc, texture_srv_heap->GetHandle(handle));

                D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
                uav_desc.Format = desc.Format;
                uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
                uav_desc.Texture2DArray.MipSlice = 0;
                uav_desc.Texture2DArray.FirstArraySlice = 0;
                uav_desc.Texture2DArray.ArraySize = desc.DepthOrArraySize;

                OffsetType uav_index = descriptor_allocator->Allocate();
                device->CreateUnorderedAccessView(cubemap_tex.Get(), nullptr, &uav_desc, descriptor_allocator->GetHandle(uav_index));

                ID3D12Resource* equirect_tex = nullptr;
                D3D12_RESOURCE_DESC equirect_desc = {};
                equirect_desc.Width = equirect_hdr_image.Width();
                equirect_desc.Height = equirect_hdr_image.Height();
                equirect_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
                equirect_desc.MipLevels = 1;
                equirect_desc.DepthOrArraySize = 1;
                equirect_desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
                equirect_desc.SampleDesc.Count = 1;
                equirect_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

               heap_properties = CD3DX12_HEAP_PROPERTIES{ D3D12_HEAP_TYPE_DEFAULT };
                BREAK_IF_FAILED(device->CreateCommittedResource(
                    &heap_properties,
                    D3D12_HEAP_FLAG_NONE,
                    &equirect_desc,
                    D3D12_RESOURCE_STATE_COMMON,
                    nullptr,
                    IID_PPV_ARGS(&equirect_tex)));

                srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srv_desc.Format = equirect_desc.Format;
                srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srv_desc.Texture2D.MipLevels = -1;
                srv_desc.Texture2D.MostDetailedMip = 0;

                OffsetType srv_index = descriptor_allocator->Allocate();
                device->CreateShaderResourceView(equirect_tex, &srv_desc, descriptor_allocator->GetHandle(srv_index));

                const UINT64 upload_buffer_size = GetRequiredIntermediateSize(equirect_tex, 0, 1);

                D3D12MA::ALLOCATION_DESC texture_upload_alloc_desc = {};
                texture_upload_alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
                D3D12_RESOURCE_DESC texture_upload_resource_desc = {};
                texture_upload_resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                texture_upload_resource_desc.Alignment = 0;
                texture_upload_resource_desc.Width = upload_buffer_size;
                texture_upload_resource_desc.Height = 1;
                texture_upload_resource_desc.DepthOrArraySize = 1;
                texture_upload_resource_desc.MipLevels = 1;
                texture_upload_resource_desc.Format = DXGI_FORMAT_UNKNOWN;
                texture_upload_resource_desc.SampleDesc.Count = 1;
                texture_upload_resource_desc.SampleDesc.Quality = 0;
                texture_upload_resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                texture_upload_resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

                D3D12MA::Allocation* texture_upload_allocation;
                BREAK_IF_FAILED(allocator->CreateResource(
                    &texture_upload_alloc_desc,
                    &texture_upload_resource_desc,
                    D3D12_RESOURCE_STATE_GENERIC_READ,
                    nullptr,
                    &texture_upload_allocation, __uuidof(nullptr), nullptr
                ));

                D3D12_SUBRESOURCE_DATA subresource_data{};
                subresource_data.pData = equirect_hdr_image.Data<void>();
                subresource_data.RowPitch = equirect_hdr_image.Pitch();

                auto r = UpdateSubresources(cmd_list, equirect_tex, texture_upload_allocation->GetResource(),
                    0, 0, 1, &subresource_data);
                ADRIA_ASSERT(r);

                gfx->AddToReleaseQueue(texture_upload_allocation);
                gfx->AddToReleaseQueue(equirect_tex);

                D3D12_RESOURCE_BARRIER barriers[] = {
                    CD3DX12_RESOURCE_BARRIER::Transition(cubemap_tex.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS), 
                    CD3DX12_RESOURCE_BARRIER::Transition(equirect_tex, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) };
                cmd_list->ResourceBarrier(_countof(barriers), barriers);

                ID3D12DescriptorHeap* pp_heaps[] = { descriptor_allocator->Heap() };
                cmd_list->SetDescriptorHeaps(1, pp_heaps);

                //Set root signature, pso and descriptor heap
                cmd_list->SetComputeRootSignature(equirect_root_signature.Get());
                cmd_list->SetPipelineState(equirect_pso.Get());

                cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetHandle(1));
                cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(0));
                cmd_list->Dispatch(1024 / 32, 1024 / 32, 6);

                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(cubemap_tex.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                cmd_list->ResourceBarrier(1, &barrier);

                //context->GenerateMips(cubemap_srv.Get());
                texture_map.insert({ handle, cubemap_tex });
            }

            return handle;
        }
        else return it->second;
    }

    [[nodiscard]]
    TEXTURE_HANDLE TextureManager::LoadCubemap(std::array<std::wstring, 6> const& cubemap_textures)
    {
        TextureFormat format = GetTextureFormat(cubemap_textures[0]);
        ADRIA_ASSERT(format == TextureFormat::eJPG || format == TextureFormat::ePNG || format == TextureFormat::eTGA ||
            format == TextureFormat::eBMP || format == TextureFormat::eHDR || format == TextureFormat::ePIC);

        auto device = gfx->GetDevice();
        auto allocator = gfx->GetAllocator();
        auto cmd_list = gfx->GetDefaultCommandList();

        ID3D12Resource* cubemap = nullptr;
        ++handle;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.MipLevels = 1;
        desc.Flags = D3D12_RESOURCE_FLAG_NONE;
        desc.DepthOrArraySize = 6;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;

        std::vector<Image> images{};
        std::vector<D3D12_SUBRESOURCE_DATA> subresources;
        for (UINT i = 0; i < cubemap_textures.size(); ++i)
        {
            images.emplace_back(ConvertToNarrow(cubemap_textures[i]), 4);
            D3D12_SUBRESOURCE_DATA subresource_data{};
            subresource_data.pData = images.back().Data<void>();
            subresource_data.RowPitch = images.back().Pitch();
            subresources.push_back(subresource_data);
        }

        desc.Width = images[0].Width();
        desc.Height = images[0].Height();
        desc.Format = images[0].IsHDR() ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;

        auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        BREAK_IF_FAILED(device->CreateCommittedResource(
            &heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&cubemap)));

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(cubemap, 0,
            static_cast<UINT>(subresources.size()));


        D3D12MA::ALLOCATION_DESC texture_upload_alloc_desc{};
        texture_upload_alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC texture_upload_resource_desc{};
        texture_upload_resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        texture_upload_resource_desc.Alignment = 0;
        texture_upload_resource_desc.Width = uploadBufferSize;
        texture_upload_resource_desc.Height = 1;
        texture_upload_resource_desc.DepthOrArraySize = 1;
        texture_upload_resource_desc.MipLevels = 1;
        texture_upload_resource_desc.Format = DXGI_FORMAT_UNKNOWN;
        texture_upload_resource_desc.SampleDesc.Count = 1;
        texture_upload_resource_desc.SampleDesc.Quality = 0;
        texture_upload_resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        texture_upload_resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12MA::Allocation* texture_upload_allocation;
        BREAK_IF_FAILED(allocator->CreateResource(
            &texture_upload_alloc_desc,
            &texture_upload_resource_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, // pOptimizedClearValue
            &texture_upload_allocation, __uuidof(nullptr), nullptr
        ));

        UpdateSubresources(cmd_list, cubemap, texture_upload_allocation->GetResource(),
            0, 0, static_cast<UINT>(subresources.size()), subresources.data());
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(cubemap,
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmd_list->ResourceBarrier(1, &barrier);

        gfx->AddToReleaseQueue(texture_upload_allocation);
        texture_map.insert({ handle, cubemap });
        CreateTextureSRV(device, texture_map[handle].Get(), texture_srv_heap->GetHandle(handle));
        return handle;
    }

    [[nodiscard]]
    D3D12_CPU_DESCRIPTOR_HANDLE TextureManager::CpuDescriptorHandle(TEXTURE_HANDLE tex_handle)
    {
        return tex_handle != INVALID_TEXTURE_HANDLE ? 
            texture_srv_heap->GetHandle((size_t)tex_handle) :
            texture_srv_heap->GetFirstHandle();
    }

    void TextureManager::TransitionTexture(TEXTURE_HANDLE handle, ResourceBarrierBatch& barrier,
        D3D12_RESOURCE_STATES state_before, D3D12_RESOURCE_STATES state_after)
    {
        barrier.AddTransition(texture_map[handle].Get(), state_before, state_after);
    }

    void TextureManager::SetMipMaps(bool mips)
    {
        mipmaps = mips;
    }

    TEXTURE_HANDLE TextureManager::LoadDDSTexture(std::wstring const& texture_path)
    {
        if (auto it = loaded_textures.find(texture_path); it == loaded_textures.end())
        {
            ++handle;
            loaded_textures.insert({ texture_path, handle });

            auto device = gfx->GetDevice();
            auto cmd_list = gfx->GetDefaultCommandList();
            auto allocator = gfx->GetAllocator();

            ID3D12Resource* tex2d = nullptr;
            std::unique_ptr<uint8_t[]> decodedData;
            std::vector<D3D12_SUBRESOURCE_DATA> subresources;

            if (false && mipmaps)
            {
                
                    HRESULT hr = DirectX::LoadDDSTextureFromFileEx(device, texture_path.data(), 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, DirectX::DDS_LOADER_MIP_RESERVE, &tex2d,
                        decodedData, subresources);
                    BREAK_IF_FAILED(hr);
            }
            else
            {
                BREAK_IF_FAILED(
                    DirectX::LoadDDSTextureFromFile(device, texture_path.data(), &tex2d,
                        decodedData, subresources));
            }

            UINT64 const upload_buffer_size = GetRequiredIntermediateSize(tex2d, 0, static_cast<UINT>(subresources.size()));

            D3D12MA::ALLOCATION_DESC texture_upload_alloc_desc{};
            texture_upload_alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC texture_upload_resource_desc{};
            texture_upload_resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            texture_upload_resource_desc.Alignment = 0;
            texture_upload_resource_desc.Width = upload_buffer_size;
            texture_upload_resource_desc.Height = 1;
            texture_upload_resource_desc.DepthOrArraySize = 1;
            texture_upload_resource_desc.MipLevels = 1;
            texture_upload_resource_desc.Format = DXGI_FORMAT_UNKNOWN;
            texture_upload_resource_desc.SampleDesc.Count = 1;
            texture_upload_resource_desc.SampleDesc.Quality = 0;
            texture_upload_resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            texture_upload_resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12MA::Allocation* texture_upload_allocation;
            BREAK_IF_FAILED(allocator->CreateResource(
                &texture_upload_alloc_desc,
                &texture_upload_resource_desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr, // pOptimizedClearValue
                &texture_upload_allocation, __uuidof(nullptr), nullptr
            ));

            UpdateSubresources(cmd_list, tex2d, texture_upload_allocation->GetResource(),
                0, 0, static_cast<UINT>(subresources.size()), subresources.data());

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(tex2d,
                D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cmd_list->ResourceBarrier(1, &barrier);

            gfx->AddToReleaseQueue(texture_upload_allocation);
            texture_map.insert({ handle, tex2d });
            if (false && mipmaps) mips_generator->Add(texture_map[handle].Get());
            CreateTextureSRV(device, texture_map[handle].Get(), texture_srv_heap->GetHandle(handle));
            return handle;
        }
        else return it->second;
    }

    TEXTURE_HANDLE TextureManager::LoadWICTexture(std::wstring const& texture_path)
    {
        if (auto it = loaded_textures.find(texture_path); it == loaded_textures.end())
        {
            auto device = gfx->GetDevice();
            auto cmd_list = gfx->GetDefaultCommandList();
            auto allocator = gfx->GetAllocator();

            ++handle;
            loaded_textures.insert({ texture_path, handle });

            ID3D12Resource* tex2d = nullptr;
            std::unique_ptr<uint8_t[]> decodedData;
            std::vector<D3D12_SUBRESOURCE_DATA> subresources;
            D3D12_SUBRESOURCE_DATA subresource{};

            if (mipmaps)
            {
                BREAK_IF_FAILED(DirectX::LoadWICTextureFromFileEx(device, texture_path.data(), 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                    DirectX::WIC_LOADER_MIP_RESERVE | DirectX::WIC_LOADER_IGNORE_SRGB | DirectX::WIC_LOADER_FORCE_RGBA32, &tex2d,
                    decodedData, subresource));
            }
            else
            {
                BREAK_IF_FAILED(DirectX::LoadWICTextureFromFile(device, texture_path.data(), &tex2d,
                    decodedData, subresource));
            }

            const UINT64 upload_buffer_size = GetRequiredIntermediateSize(tex2d, 0, 1);
            D3D12MA::ALLOCATION_DESC texture_upload_alloc_desc{};
            texture_upload_alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC texture_upload_resource_desc{};
            texture_upload_resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            texture_upload_resource_desc.Alignment = 0;
            texture_upload_resource_desc.Width = upload_buffer_size;
            texture_upload_resource_desc.Height = 1;
            texture_upload_resource_desc.DepthOrArraySize = 1;
            texture_upload_resource_desc.MipLevels = 1;
            texture_upload_resource_desc.Format = DXGI_FORMAT_UNKNOWN;
            texture_upload_resource_desc.SampleDesc.Count = 1;
            texture_upload_resource_desc.SampleDesc.Quality = 0;
            texture_upload_resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            texture_upload_resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12MA::Allocation* texture_upload_allocation;
            BREAK_IF_FAILED(allocator->CreateResource(
                &texture_upload_alloc_desc,
                &texture_upload_resource_desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr, // pOptimizedClearValue
                &texture_upload_allocation, __uuidof(nullptr), nullptr
            ));

            UpdateSubresources(cmd_list, tex2d, texture_upload_allocation->GetResource(),
                0, 0, 1, &subresource);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(tex2d,
                D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cmd_list->ResourceBarrier(1, &barrier);
            gfx->AddToReleaseQueue(texture_upload_allocation);
            texture_map.insert({ handle, tex2d });
            if (mipmaps) mips_generator->Add(texture_map[handle].Get());
            CreateTextureSRV(device, texture_map[handle].Get(), texture_srv_heap->GetHandle(handle));
            return handle;
        }
        else return it->second;
    }

    TEXTURE_HANDLE TextureManager::LoadTexture_HDR_TGA_PIC(std::string const& texture_path)
    {
        if (auto it = loaded_textures.find(texture_path); it == loaded_textures.end())
        {
            auto device = gfx->GetDevice();
            auto cmd_list = gfx->GetDefaultCommandList();
            auto allocator = gfx->GetAllocator();

            ++handle;
            loaded_textures.insert({ texture_path, handle });
            ID3D12Resource* tex2d = nullptr;
            Image img(texture_path, 4);

            D3D12_RESOURCE_DESC desc{};
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            desc.MipLevels = mipmaps ? 0 : 1;
            desc.Format = img.IsHDR() ? DXGI_FORMAT_R32G32B32A32_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.Width = img.Width();
            desc.Height = img.Height();
            desc.Flags = mipmaps ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
            desc.DepthOrArraySize = 1;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

            auto heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            BREAK_IF_FAILED(device->CreateCommittedResource(
                &heap_properties,
                D3D12_HEAP_FLAG_NONE,
                &desc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&tex2d)));

           UINT64 const upload_buffer_size = GetRequiredIntermediateSize(tex2d, 0, 1);

            D3D12MA::ALLOCATION_DESC texture_upload_alloc_desc{};
            texture_upload_alloc_desc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC texture_upload_resource_desc{};
            texture_upload_resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            texture_upload_resource_desc.Alignment = 0;
            texture_upload_resource_desc.Width = upload_buffer_size;
            texture_upload_resource_desc.Height = 1;
            texture_upload_resource_desc.DepthOrArraySize = 1;
            texture_upload_resource_desc.MipLevels = 1;
            texture_upload_resource_desc.Format = DXGI_FORMAT_UNKNOWN;
            texture_upload_resource_desc.SampleDesc.Count = 1;
            texture_upload_resource_desc.SampleDesc.Quality = 0;
            texture_upload_resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            texture_upload_resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12MA::Allocation* texture_upload_allocation;
            BREAK_IF_FAILED(allocator->CreateResource(
                &texture_upload_alloc_desc,
                &texture_upload_resource_desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                &texture_upload_allocation, __uuidof(nullptr), nullptr
            ));
            D3D12_SUBRESOURCE_DATA data{};
            data.pData = img.Data<void>();
            data.RowPitch = img.Pitch();

            UpdateSubresources(cmd_list, tex2d, texture_upload_allocation->GetResource(),
                0, 0, 1u, &data);
            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(tex2d,
                D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cmd_list->ResourceBarrier(1, &barrier);
            gfx->AddToReleaseQueue(texture_upload_allocation);
            texture_map.insert({ handle, tex2d });
            if (mipmaps) mips_generator->Add(texture_map[handle].Get());
            CreateTextureSRV(device, texture_map[handle].Get(), texture_srv_heap->GetHandle(handle));
            return handle;
        }
        else return it->second;

    }

}