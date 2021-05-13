#include "TextureManager.h"
#include "GraphicsCoreDX12.h"
#include "d3dx12.h"
#include "DDSTextureLoader12.h"
#include "WICTextureLoader12.h"

#include "../Utilities/StringUtil.h"
#include "../Utilities/Image.h"
#include "../Core/Macros.h"


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
            std::string extension = path.substr(path.find_last_of(".") + 1);

            if (extension == "dds")
                return TextureFormat::eDDS;
            else if (extension == "bmp")
                return TextureFormat::eBMP;
            else if (extension == "jpg" || extension == "jpeg")
                return TextureFormat::eJPG;
            else if (extension == "png")
                return TextureFormat::ePNG;
            else if (extension == "tiff" || extension == "tif")
                return TextureFormat::eTIFF;
            else if (extension == "gif")
                return TextureFormat::eGIF;
            else if (extension == "ico")
                return TextureFormat::eICO;
            else if (extension == "tga")
                return TextureFormat::eTGA;
            else if (extension == "hdr")
                return TextureFormat::eHDR;
            else if (extension == "pic")
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
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

            if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
            {

                if (desc.DepthOrArraySize == 6)
                {
                    srvDesc.TextureCube.MostDetailedMip = 0;
                    srvDesc.TextureCube.MipLevels = -1;
                    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
                }
                else
                {
                    srvDesc.Texture2D.MostDetailedMip = 0;
                    srvDesc.Texture2D.MipLevels = -1;
                    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
                }

                srvDesc.Format = desc.Format;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.ViewDimension = desc.DepthOrArraySize == 6 ? D3D12_SRV_DIMENSION_TEXTURECUBE : D3D12_SRV_DIMENSION_TEXTURE2D;

                
            }
            else if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
            {
                srvDesc.Format = desc.Format;
                srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
                srvDesc.Texture3D.MipLevels = -1;
                srvDesc.Texture3D.MostDetailedMip = 0;
                srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;
            }

            device->CreateShaderResourceView(tex, &srvDesc, cpu_handle);

        }

        void CreateNullSRV(ID3D12Device* device, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle) //bool is_cube
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = -1;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

            srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

            device->CreateShaderResourceView(nullptr, &srvDesc, cpu_handle);
        }

	}


    TextureManager::TextureManager(GraphicsCoreDX12* gfx, UINT max_textures) : gfx(gfx)
    {
        D3D12_DESCRIPTOR_HEAP_DESC texture_heap_desc = {};
        texture_heap_desc.NumDescriptors = max_textures;
        texture_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        texture_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; 
        texture_srv_heap.reset(new DescriptorHeap(gfx->Device(), texture_heap_desc));
        mips_generator = std::make_unique<MipsGenerator>(gfx->Device(), max_textures);

        CreateNullSRV(gfx->Device(), texture_srv_heap->GetFirstCpuHandle());
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
        ADRIA_ASSERT(format == TextureFormat::eDDS && "One file cubemap has to be .dds file!");

        if (auto it = loaded_textures.find(name); it == loaded_textures.end())
        {
            ++handle;

            ADRIA_ASSERT(texture_srv_heap->Count() > handle && "Not enough space for descriptors in Texture Cache");

            loaded_textures.insert({ name, handle });

            auto device = gfx->Device();
            auto allocator = gfx->Allocator();
            auto cmd_list = gfx->DefaultCommandList();

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


            D3D12MA::ALLOCATION_DESC textureUploadAllocDesc = {};
            textureUploadAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC textureUploadResourceDesc = {};
            textureUploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            textureUploadResourceDesc.Alignment = 0;
            textureUploadResourceDesc.Width = uploadBufferSize;
            textureUploadResourceDesc.Height = 1;
            textureUploadResourceDesc.DepthOrArraySize = 1;
            textureUploadResourceDesc.MipLevels = 1;
            textureUploadResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            textureUploadResourceDesc.SampleDesc.Count = 1;
            textureUploadResourceDesc.SampleDesc.Quality = 0;
            textureUploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            textureUploadResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            //Microsoft::WRL::ComPtr<ID3D12Resource> textureUpload;

            D3D12MA::Allocation* textureUploadAllocation;
            BREAK_IF_FAILED(allocator->CreateResource(
                &textureUploadAllocDesc,
                &textureUploadResourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr, // pOptimizedClearValue
                &textureUploadAllocation, __uuidof(nullptr), nullptr
            ));

            UpdateSubresources(cmd_list, cubemap, textureUploadAllocation->GetResource(),
                0, 0, static_cast<UINT>(subresources.size()), subresources.data());

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(cubemap,
                D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cmd_list->ResourceBarrier(1, &barrier);

            gfx->AddToReleaseQueue(textureUploadAllocation);

            texture_map.insert({ handle, cubemap });

            CreateTextureSRV(device, texture_map[handle].Get(), texture_srv_heap->GetCpuHandle(handle));
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

        auto device = gfx->Device();
        auto allocator = gfx->Allocator();
        auto cmd_list = gfx->DefaultCommandList();

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


        D3D12MA::ALLOCATION_DESC textureUploadAllocDesc = {};
        textureUploadAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC textureUploadResourceDesc = {};
        textureUploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        textureUploadResourceDesc.Alignment = 0;
        textureUploadResourceDesc.Width = uploadBufferSize;
        textureUploadResourceDesc.Height = 1;
        textureUploadResourceDesc.DepthOrArraySize = 1;
        textureUploadResourceDesc.MipLevels = 1;
        textureUploadResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        textureUploadResourceDesc.SampleDesc.Count = 1;
        textureUploadResourceDesc.SampleDesc.Quality = 0;
        textureUploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        textureUploadResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12MA::Allocation* textureUploadAllocation;
        BREAK_IF_FAILED(allocator->CreateResource(
            &textureUploadAllocDesc,
            &textureUploadResourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, // pOptimizedClearValue
            &textureUploadAllocation, __uuidof(nullptr), nullptr
        ));

        UpdateSubresources(cmd_list, cubemap, textureUploadAllocation->GetResource(),
            0, 0, static_cast<UINT>(subresources.size()), subresources.data());

        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(cubemap,
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmd_list->ResourceBarrier(1, &barrier);

        gfx->AddToReleaseQueue(textureUploadAllocation);

        texture_map.insert({ handle, cubemap });

        CreateTextureSRV(device, texture_map[handle].Get(), texture_srv_heap->GetCpuHandle(handle));

        return handle;
    }

    [[nodiscard]]
    D3D12_CPU_DESCRIPTOR_HANDLE TextureManager::CpuDescriptorHandle(TEXTURE_HANDLE tex_handle)
    {
        return tex_handle != INVALID_TEXTURE_HANDLE ? 
            texture_srv_heap->GetCpuHandle((size_t)tex_handle) :
            texture_srv_heap->GetFirstCpuHandle();
    }

    void TextureManager::TransitionTexture(TEXTURE_HANDLE handle, ResourceBarriers& barrier,
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

            auto device = gfx->Device();
            auto cmd_list = gfx->DefaultCommandList();
            auto allocator = gfx->Allocator();

            ID3D12Resource* tex2d = nullptr;
            std::unique_ptr<uint8_t[]> decodedData;
            std::vector<D3D12_SUBRESOURCE_DATA> subresources;

            if (mipmaps)
            {
                BREAK_IF_FAILED(
                    DirectX::LoadDDSTextureFromFileEx(device, texture_path.data(), 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, DirectX::DDS_LOADER_MIP_RESERVE, &tex2d,
                        decodedData, subresources));
            }
            else
            {
                BREAK_IF_FAILED(
                    DirectX::LoadDDSTextureFromFile(device, texture_path.data(), &tex2d,
                        decodedData, subresources));
            }


            const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex2d, 0, static_cast<UINT>(subresources.size()));

            D3D12MA::ALLOCATION_DESC textureUploadAllocDesc = {};
            textureUploadAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC textureUploadResourceDesc = {};
            textureUploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            textureUploadResourceDesc.Alignment = 0;
            textureUploadResourceDesc.Width = uploadBufferSize;
            textureUploadResourceDesc.Height = 1;
            textureUploadResourceDesc.DepthOrArraySize = 1;
            textureUploadResourceDesc.MipLevels = 1;
            textureUploadResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            textureUploadResourceDesc.SampleDesc.Count = 1;
            textureUploadResourceDesc.SampleDesc.Quality = 0;
            textureUploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            textureUploadResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;


            D3D12MA::Allocation* textureUploadAllocation;
            BREAK_IF_FAILED(allocator->CreateResource(
                &textureUploadAllocDesc,
                &textureUploadResourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr, // pOptimizedClearValue
                &textureUploadAllocation, __uuidof(nullptr), nullptr
            ));


            UpdateSubresources(cmd_list, tex2d, textureUploadAllocation->GetResource(),
                0, 0, static_cast<UINT>(subresources.size()), subresources.data());


            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(tex2d,
                D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cmd_list->ResourceBarrier(1, &barrier);

            gfx->AddToReleaseQueue(textureUploadAllocation);

            texture_map.insert({ handle, tex2d });

            if (mipmaps) mips_generator->Add(texture_map[handle].Get());

            CreateTextureSRV(device, texture_map[handle].Get(), texture_srv_heap->GetCpuHandle(handle));

            return handle;
        }
        else return it->second;
    }

    TEXTURE_HANDLE TextureManager::LoadWICTexture(std::wstring const& texture_path)
    {
        if (auto it = loaded_textures.find(texture_path); it == loaded_textures.end())
        {

            auto device = gfx->Device();
            auto cmd_list = gfx->DefaultCommandList();
            auto allocator = gfx->Allocator();

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


            const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex2d, 0, 1);

            D3D12MA::ALLOCATION_DESC textureUploadAllocDesc = {};
            textureUploadAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC textureUploadResourceDesc = {};
            textureUploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            textureUploadResourceDesc.Alignment = 0;
            textureUploadResourceDesc.Width = uploadBufferSize;
            textureUploadResourceDesc.Height = 1;
            textureUploadResourceDesc.DepthOrArraySize = 1;
            textureUploadResourceDesc.MipLevels = 1;
            textureUploadResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            textureUploadResourceDesc.SampleDesc.Count = 1;
            textureUploadResourceDesc.SampleDesc.Quality = 0;
            textureUploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            textureUploadResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12MA::Allocation* textureUploadAllocation;
            BREAK_IF_FAILED(allocator->CreateResource(
                &textureUploadAllocDesc,
                &textureUploadResourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr, // pOptimizedClearValue
                &textureUploadAllocation, __uuidof(nullptr), nullptr
            ));

            UpdateSubresources(cmd_list, tex2d, textureUploadAllocation->GetResource(),
                0, 0, 1, &subresource);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(tex2d,
                D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cmd_list->ResourceBarrier(1, &barrier);

            gfx->AddToReleaseQueue(textureUploadAllocation);

            texture_map.insert({ handle, tex2d });

            if (mipmaps) mips_generator->Add(texture_map[handle].Get());

            CreateTextureSRV(device, texture_map[handle].Get(), texture_srv_heap->GetCpuHandle(handle));

            return handle;
        }
        else return it->second;
    }

    TEXTURE_HANDLE TextureManager::LoadTexture_HDR_TGA_PIC(std::string const& texture_path)
    {
        if (auto it = loaded_textures.find(texture_path); it == loaded_textures.end())
        {
            auto device = gfx->Device();
            auto cmd_list = gfx->DefaultCommandList();
            auto allocator = gfx->Allocator();

            ++handle;
            loaded_textures.insert({ texture_path, handle });

            ID3D12Resource* tex2d = nullptr;

            Image img(texture_path, 4);

            D3D12_RESOURCE_DESC desc{};

            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            // Describe and create a Texture2D.

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

            const UINT64 uploadBufferSize = GetRequiredIntermediateSize(tex2d, 0, 1);


            D3D12MA::ALLOCATION_DESC textureUploadAllocDesc = {};
            textureUploadAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;
            D3D12_RESOURCE_DESC textureUploadResourceDesc = {};
            textureUploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            textureUploadResourceDesc.Alignment = 0;
            textureUploadResourceDesc.Width = uploadBufferSize;
            textureUploadResourceDesc.Height = 1;
            textureUploadResourceDesc.DepthOrArraySize = 1;
            textureUploadResourceDesc.MipLevels = 1;
            textureUploadResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            textureUploadResourceDesc.SampleDesc.Count = 1;
            textureUploadResourceDesc.SampleDesc.Quality = 0;
            textureUploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            textureUploadResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12MA::Allocation* textureUploadAllocation;
            BREAK_IF_FAILED(allocator->CreateResource(
                &textureUploadAllocDesc,
                &textureUploadResourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                &textureUploadAllocation, __uuidof(nullptr), nullptr
            ));

            D3D12_SUBRESOURCE_DATA data{};
            data.pData = img.Data<void>();
            data.RowPitch = img.Pitch();

            UpdateSubresources(cmd_list, tex2d, textureUploadAllocation->GetResource(),
                0, 0, 1u, &data);

            auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(tex2d,
                D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            cmd_list->ResourceBarrier(1, &barrier);

            gfx->AddToReleaseQueue(textureUploadAllocation);

            texture_map.insert({ handle, tex2d });

            if (mipmaps) mips_generator->Add(texture_map[handle].Get());

            CreateTextureSRV(device, texture_map[handle].Get(), texture_srv_heap->GetCpuHandle(handle));

            return handle;
        }
        else return it->second;

    }

}