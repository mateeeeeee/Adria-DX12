#include "DirectXTex.h"
#ifdef _DEBUG
#pragma comment(lib, "Debug\\DirectXTex.lib")
#else
#pragma comment(lib, "Release\\DirectXTex.lib")
#endif
#include "d3dx12.h"
#include "DDSTextureLoader12.h"
#include "WICTextureLoader12.h"

#include "TextureManager.h"
#include "MipsGenerator.h"
#include "../Graphics/GfxTexture.h"
#include "../Graphics/GfxDevice.h"
#include "../Graphics/GfxCommandList.h"
#include "../Graphics/GfxRingDescriptorAllocator.h"
#include "../Graphics/GfxShaderCompiler.h"
#include "../Core/Macros.h"
#include "../Logging/Logger.h"
#include "../Utilities/StringUtil.h"
#include "../Utilities/Image.h"
#include "../Utilities/FilesUtil.h"

namespace adria
{

	namespace
	{
        enum class TextureFormat
        {
            DDS,
            BMP,
            JPG,
            PNG,
            TIFF,
            GIF,
            ICO,
            TGA,
            HDR,
            PIC,
            NotSupported
        };
        TextureFormat GetTextureFormat(std::string const& path)
        {
            std::string extension = GetExtension(path);
            std::transform(std::begin(extension), std::end(extension), std::begin(extension), [](char c) {return std::tolower(c); });

            if (extension == ".dds")
                return TextureFormat::DDS;
            else if (extension == ".bmp")
                return TextureFormat::BMP;
            else if (extension == ".jpg" || extension == ".jpeg")
                return TextureFormat::JPG;
            else if (extension == ".png")
                return TextureFormat::PNG;
            else if (extension == ".tiff" || extension == ".tif")
                return TextureFormat::TIFF;
            else if (extension == ".gif")
                return TextureFormat::GIF;
            else if (extension == ".ico")
                return TextureFormat::ICO;
            else if (extension == ".tga")
                return TextureFormat::TGA;
            else if (extension == ".hdr")
                return TextureFormat::HDR;
            else if (extension == ".pic")
                return TextureFormat::PIC;
            else
                return TextureFormat::NotSupported;
        }
        TextureFormat GetTextureFormat(std::wstring const& path)
        {
            return GetTextureFormat(ToString(path));
        }
	}

    TextureManager::TextureManager() {}

    TextureManager::~TextureManager() = default;

	TextureManager& TextureManager::Get()
	{
		static TextureManager tex_manager;
		return tex_manager;
	}

	void TextureManager::Initialize(GfxDevice* _gfx, UINT max_textures)
	{
        gfx = _gfx;
		mips_generator = std::make_unique<MipsGenerator>(gfx, max_textures);

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
		ArcPtr<ID3DBlob> signature;
		ArcPtr<ID3DBlob> error;
		HRESULT hr = D3DX12SerializeVersionedRootSignature(&signature_desc, D3D_ROOT_SIGNATURE_VERSION_1_1, signature.GetAddressOf(), error.GetAddressOf());
		if (error) ADRIA_LOG(ERROR, (char const*)error->GetBufferPointer());
		BREAK_IF_FAILED(gfx->GetDevice()->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(equirect_root_signature.GetAddressOf())));

		GfxShaderBlob equirect_cs_shader;
		GfxShaderCompiler::ReadBlobFromFile(L"Resources/Compiled Shaders/Equirect2cubeCS.cso", equirect_cs_shader);

		D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{};
		pso_desc.pRootSignature = equirect_root_signature.Get();
		pso_desc.CS = D3D12_SHADER_BYTECODE{ .pShaderBytecode = equirect_cs_shader.data(), .BytecodeLength = equirect_cs_shader.size() };
		BREAK_IF_FAILED(gfx->GetDevice()->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(equirect_pso.GetAddressOf())));
	}

	void TextureManager::Destroy()
	{
        texture_map.clear();
        loaded_textures.clear();
        mips_generator.reset();
        gfx = nullptr;
        equirect_root_signature = nullptr;
        equirect_pso = nullptr;
	}

	void TextureManager::Tick()
	{
		mips_generator->Generate(gfx->GetGraphicsCommandList());
	}

    [[nodiscard]]
    TextureHandle TextureManager::LoadTexture(std::wstring const& name)
    {
        TextureFormat format = GetTextureFormat(name);

        switch (format)
        {
        case TextureFormat::DDS:
            return LoadDDSTexture(name);
        case TextureFormat::BMP:
        case TextureFormat::PNG:
        case TextureFormat::JPG:
        case TextureFormat::TIFF:
        case TextureFormat::GIF:
        case TextureFormat::ICO:
            return LoadWICTexture(name);
        case TextureFormat::TGA:
        case TextureFormat::HDR:
        case TextureFormat::PIC:
            return LoadTexture_HDR_TGA_PIC(ToString(name));
        case TextureFormat::NotSupported:
        default:
            ADRIA_ASSERT(false && "Unsupported Texture Format!");
        }
        return INVALID_TEXTURE_HANDLE;
    }

    [[nodiscard]]
    TextureHandle TextureManager::LoadCubemap(std::wstring const& name)
    {
        TextureFormat format = GetTextureFormat(name);
        ADRIA_ASSERT(format == TextureFormat::DDS || format == TextureFormat::HDR && "Cubemap in one file has to be .dds or .hdr format");

        if (auto it = loaded_textures.find(name); it == loaded_textures.end())
        {
            ++handle;
            auto device = gfx->GetDevice();
            auto allocator = gfx->GetAllocator();
            auto cmd_list = gfx->GetCommandList();
            if (format == TextureFormat::DDS)
            {
                loaded_textures.insert({ name, handle });
                ArcPtr<ID3D12Resource> cubemap = nullptr;
                std::unique_ptr<uint8_t[]> decoded_data;
                std::vector<GfxTextureInitialData> subresources;

                bool is_cubemap;
                BREAK_IF_FAILED(
                    DirectX::LoadDDSTextureFromFile(device, name.c_str(), cubemap.GetAddressOf(),
                        decoded_data, subresources, 0, nullptr, &is_cubemap));
                ADRIA_ASSERT(is_cubemap);

				D3D12_RESOURCE_DESC _desc = cubemap->GetDesc();
				GfxTextureDesc desc{};
				desc.type = GfxTextureType_2D;
                desc.misc_flags = GfxTextureMiscFlag::TextureCube;
				desc.width = (uint32)_desc.Width;
				desc.height = _desc.Height;
				desc.array_size = 6;
				desc.bind_flags = GfxBindFlag::ShaderResource;
				desc.format = ConvertDXGIFormat(_desc.Format);
				desc.initial_state = GfxResourceState::PixelShaderResource | GfxResourceState::NonPixelShaderResource;
				desc.heap_type = GfxResourceUsage::Default;
				desc.mip_levels = _desc.MipLevels;

				std::unique_ptr<GfxTexture> tex = std::make_unique<GfxTexture>(gfx, desc, subresources.data());
                texture_map.insert({ handle, std::move(tex)});
                CreateViewForTexture(handle);
            }
            else //format == TextureFormat::eHDR //#todo
            {
                auto descriptor_allocator = gfx->GetDescriptorAllocator();
                loaded_textures.insert({ name, handle });
                Image equirect_hdr_image(ToString(name));

                GfxTextureDesc desc;
                desc.type = GfxTextureType_2D;
                desc.misc_flags = GfxTextureMiscFlag::TextureCube;
                desc.heap_type = GfxResourceUsage::Default;
                desc.width = 1024;
                desc.height = 1024;
                desc.array_size = 6;
                desc.mip_levels = 1;
                desc.format = GfxFormat::R16G16B16A16_FLOAT;
                desc.bind_flags = GfxBindFlag::ShaderResource | GfxBindFlag::UnorderedAccess;
                desc.initial_state = GfxResourceState::Common;

                std::unique_ptr<GfxTexture> cubemap_tex = std::make_unique<GfxTexture>(gfx, desc);
                //cubemap_tex->CreateSRV();
                //cubemap_tex->CreateUAV();

                GfxTextureDesc equirect_desc{};
                equirect_desc.width = equirect_hdr_image.Width();
                equirect_desc.height = equirect_hdr_image.Height();
                equirect_desc.type = GfxTextureType_2D;
                equirect_desc.mip_levels = 1;
                equirect_desc.array_size = 1;
                equirect_desc.format = GfxFormat::R32G32B32A32_FLOAT;
                equirect_desc.bind_flags = GfxBindFlag::ShaderResource;
                equirect_desc.initial_state = GfxResourceState::CopyDest;

                GfxTextureInitialData subresource_data{};
                subresource_data.pData = equirect_hdr_image.Data<void>();
                subresource_data.RowPitch = equirect_hdr_image.Pitch();

				GfxTexture equirect_tex(gfx, desc,&subresource_data);
                //equirect_tex.CreateSRV();

                D3D12_RESOURCE_BARRIER barriers[] = {
                    CD3DX12_RESOURCE_BARRIER::Transition(cubemap_tex->GetNative(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS),
                    CD3DX12_RESOURCE_BARRIER::Transition(equirect_tex.GetNative(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) };
                cmd_list->ResourceBarrier(_countof(barriers), barriers);

                ID3D12DescriptorHeap* heap = descriptor_allocator->GetHeap();
                cmd_list->SetDescriptorHeaps(1, &heap);

                //Set root signature, pso and descriptor heap
                cmd_list->SetComputeRootSignature(equirect_root_signature.Get());
                cmd_list->SetPipelineState(equirect_pso.Get());

                cmd_list->SetComputeRootDescriptorTable(0, descriptor_allocator->GetHandle(1));
                cmd_list->SetComputeRootDescriptorTable(1, descriptor_allocator->GetHandle(0));
                cmd_list->Dispatch(1024 / 32, 1024 / 32, 6);

                auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(cubemap_tex->GetNative(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
                cmd_list->ResourceBarrier(1, &barrier);

                texture_map.insert({ handle, std::move(cubemap_tex)});
                CreateViewForTexture(handle);
            }

            return handle;
        }
        else return it->second;
    }

    [[nodiscard]]
    TextureHandle TextureManager::LoadCubemap(std::array<std::wstring, 6> const& cubemap_textures)
    {
        TextureFormat format = GetTextureFormat(cubemap_textures[0]);
        ADRIA_ASSERT(format == TextureFormat::JPG || format == TextureFormat::PNG || format == TextureFormat::TGA ||
            format == TextureFormat::BMP || format == TextureFormat::HDR || format == TextureFormat::PIC);

        auto device = gfx->GetDevice();
        auto allocator = gfx->GetAllocator();
        auto cmd_list = gfx->GetCommandList();

        ++handle;
        GfxTextureDesc desc{};
        desc.type = GfxTextureType_2D;
        desc.mip_levels = 1;
        desc.misc_flags = GfxTextureMiscFlag::TextureCube;
        desc.array_size = 6;
        desc.bind_flags = GfxBindFlag::ShaderResource;

        std::vector<Image> images{};
        std::vector<GfxTextureInitialData> subresources;
        for (UINT i = 0; i < cubemap_textures.size(); ++i)
        {
            images.emplace_back(ToString(cubemap_textures[i]), 4);
            GfxTextureInitialData subresource_data{};
            subresource_data.pData = images.back().Data<void>();
            subresource_data.RowPitch = images.back().Pitch();
            subresources.push_back(subresource_data);
        }
        desc.width = images[0].Width();
        desc.height = images[0].Height();
        desc.format = images[0].IsHDR() ? GfxFormat::R32G32B32A32_FLOAT : GfxFormat::R8G8B8A8_UNORM;
        std::unique_ptr<GfxTexture> cubemap = std::make_unique<GfxTexture>(gfx, desc, subresources.data());
        texture_map.insert({ handle, std::move(cubemap)});
        CreateViewForTexture(handle);
        return handle;
    }

    [[nodiscard]]
	GfxDescriptor TextureManager::GetSRV(TextureHandle tex_handle)
	{
		return texture_srv_map[tex_handle];
	}

    [[nodiscard]]
	GfxTexture* TextureManager::GetTexture(TextureHandle handle) const
	{
		if (handle == INVALID_TEXTURE_HANDLE) return nullptr;
		else if (auto it = texture_map.find(handle); it != texture_map.end()) return it->second.get();
		else return nullptr;
	}

	void TextureManager::EnableMipMaps(bool mips)
    {
        mipmaps = mips;
    }

	void TextureManager::OnSceneInitialized()
	{
		GfxTextureDesc desc{};
		desc.width = 1;
		desc.height = 1;
		desc.format = GfxFormat::R32_FLOAT;
		desc.bind_flags = GfxBindFlag::ShaderResource;
		desc.initial_state = GfxResourceState::AllShaderResource;

		float v = 0.0f;
		GfxTextureInitialData init_data{};
		init_data.pData = &v;
		init_data.RowPitch = sizeof(float);
		init_data.SlicePitch = 0;
		std::unique_ptr<GfxTexture> black_default_texture = std::make_unique<GfxTexture>(gfx, desc, &init_data);
		texture_map[INVALID_TEXTURE_HANDLE] = std::move(black_default_texture);

		mips_generator->Generate(gfx->GetGraphicsCommandList());

		gfx->InitShaderVisibleAllocator(1024);
		ID3D12Device* device = gfx->GetDevice();
		auto* online_descriptor_allocator = gfx->GetDescriptorAllocator();
        for (size_t i = 0; i <= handle; ++i)
        {
            GfxTexture* texture = texture_map[TextureHandle(i)].get();
            if (texture)
            {
                CreateViewForTexture(TextureHandle(i), true);
            }
        }
        is_scene_initialized = true;
	}

	TextureHandle TextureManager::LoadDDSTexture(std::wstring const& texture_path)
    {
        if (auto it = loaded_textures.find(texture_path); it == loaded_textures.end())
        {
            ++handle;
            loaded_textures.insert({ texture_path, handle });

            auto device = gfx->GetDevice();
            auto cmd_list = gfx->GetCommandList();
            auto allocator = gfx->GetAllocator();

            ArcPtr<ID3D12Resource> tex2d = nullptr;
            std::unique_ptr<uint8_t[]> decoded_data;
            std::vector<GfxTextureInitialData> subresources;
			BREAK_IF_FAILED(
				DirectX::LoadDDSTextureFromFile(device, texture_path.data(), tex2d.GetAddressOf(),
					decoded_data, subresources));

            D3D12_RESOURCE_DESC _desc = tex2d->GetDesc();
            GfxTextureDesc desc{};
            desc.type = ConvertTextureType(_desc.Dimension);
            desc.misc_flags = GfxTextureMiscFlag::None;
            desc.width = (uint32)_desc.Width;
            desc.height = _desc.Height;
            desc.array_size = _desc.DepthOrArraySize;
            desc.depth = _desc.DepthOrArraySize;
            desc.bind_flags = GfxBindFlag::ShaderResource;

            desc.format = ConvertDXGIFormat(_desc.Format);
            desc.initial_state = GfxResourceState::PixelShaderResource | GfxResourceState::NonPixelShaderResource;
            desc.heap_type = GfxResourceUsage::Default;
            desc.mip_levels = _desc.MipLevels;
            std::unique_ptr<GfxTexture> tex = std::make_unique<GfxTexture>(gfx, desc, subresources.data(), (uint32)subresources.size());

            texture_map.insert({ handle, std::move(tex)});
            CreateViewForTexture(handle);
            return handle;
        }
        else return it->second;
    }

    TextureHandle TextureManager::LoadWICTexture(std::wstring const& texture_path)
    {
        if (auto it = loaded_textures.find(texture_path); it == loaded_textures.end())
        {
            auto device = gfx->GetDevice();
            auto cmd_list = gfx->GetCommandList();
            auto allocator = gfx->GetAllocator();

            ++handle;
            loaded_textures.insert({ texture_path, handle });

            ArcPtr<ID3D12Resource> d3d12_tex = nullptr;
            std::unique_ptr<uint8_t[]> decoded_data;
            GfxTextureInitialData subresource{};
            if (mipmaps)
            {
                BREAK_IF_FAILED(DirectX::LoadWICTextureFromFileEx(device, texture_path.data(), 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                    DirectX::WIC_LOADER_MIP_RESERVE | DirectX::WIC_LOADER_IGNORE_SRGB | DirectX::WIC_LOADER_FORCE_RGBA32, d3d12_tex.GetAddressOf(),
                    decoded_data, subresource));
            }
            else
            {
                BREAK_IF_FAILED(DirectX::LoadWICTextureFromFile(device, texture_path.data(), d3d12_tex.GetAddressOf(),
                    decoded_data, subresource));
            }
			D3D12_RESOURCE_DESC _desc = d3d12_tex->GetDesc();
			GfxTextureDesc desc{};
			desc.type = GfxTextureType_2D;
			desc.misc_flags = GfxTextureMiscFlag::None;
			desc.width = (uint32)_desc.Width;
			desc.height = _desc.Height;
			desc.array_size = _desc.DepthOrArraySize;
			desc.depth = _desc.DepthOrArraySize;
            desc.bind_flags = GfxBindFlag::ShaderResource;
            if (mipmaps && _desc.MipLevels != 1)
            {
                desc.bind_flags |= GfxBindFlag::UnorderedAccess;
            }
			desc.format = ConvertDXGIFormat(_desc.Format);
			desc.initial_state = GfxResourceState::PixelShaderResource | GfxResourceState::NonPixelShaderResource;
			desc.heap_type = GfxResourceUsage::Default;
			desc.mip_levels = _desc.MipLevels;
			std::unique_ptr<GfxTexture> tex = std::make_unique<GfxTexture>(gfx, desc, &subresource, 1);

            texture_map.insert({ handle, std::move(tex)});
            if (mipmaps) mips_generator->Add(texture_map[handle].get());
            CreateViewForTexture(handle);
            return handle;
        }
        else return it->second;
    }

    TextureHandle TextureManager::LoadTexture_HDR_TGA_PIC(std::string const& texture_path)
    {
        if (auto it = loaded_textures.find(texture_path); it == loaded_textures.end())
        {
            auto device = gfx->GetDevice();
            auto cmd_list = gfx->GetCommandList();
            auto allocator = gfx->GetAllocator();
            ++handle;
			loaded_textures.insert({ texture_path, handle });
            Image img(texture_path, 4);

			GfxTextureDesc desc{};
			desc.type = GfxTextureType_2D;
			desc.misc_flags = GfxTextureMiscFlag::None;
			desc.width = img.Width();
			desc.height = img.Height();
			desc.array_size = 1;
			desc.depth = 1;
			desc.bind_flags = GfxBindFlag::ShaderResource;
			if (mipmaps)
			{
				desc.bind_flags |= GfxBindFlag::UnorderedAccess;
			}
			desc.format = img.IsHDR() ? GfxFormat::R32G32B32A32_FLOAT : GfxFormat::R8G8B8A8_UNORM;
			desc.initial_state = GfxResourceState::PixelShaderResource | GfxResourceState::NonPixelShaderResource;
			desc.heap_type = GfxResourceUsage::Default;
			desc.mip_levels = mipmaps ? 0 : 1;

			GfxTextureInitialData data{};
			data.pData = img.Data<void>();
			data.RowPitch = img.Pitch();

			std::unique_ptr<GfxTexture> tex = std::make_unique<GfxTexture>(gfx, desc, &data);
            texture_map.insert({ handle, std::move(tex)});
            if (mipmaps) mips_generator->Add(texture_map[handle].get());
            CreateViewForTexture(handle);
            return handle;
        }
        else return it->second;

    }

	void TextureManager::CreateViewForTexture(TextureHandle handle, bool flag)
	{
        if (!is_scene_initialized && !flag) return;

		auto* online_descriptor_allocator = gfx->GetDescriptorAllocator();
		GfxTexture* texture = texture_map[handle].get();
		ADRIA_ASSERT(texture);
        texture_srv_map[handle] = gfx->CreateTextureSRV(texture);
        gfx->CopyDescriptors(1, online_descriptor_allocator->GetHandle((uint32)handle),
            texture_srv_map[handle]);
	}

}