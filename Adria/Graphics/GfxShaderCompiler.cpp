#pragma comment(lib, "dxcompiler.lib")
#include "GfxShaderCompiler.h"
#include <d3dcompiler.h> 
#include "dxc/dxcapi.h" 
#include "cereal/archives/binary.hpp"
#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "../Utilities/StringUtil.h"
#include "../Utilities/FilesUtil.h"
#include "../Utilities/HashUtil.h"
#include "../Utilities/AutoRefCountPtr.h"
#include "../Logging/Logger.h"


namespace adria
{
	namespace
	{
		ArcPtr<IDxcLibrary> library = nullptr;
		ArcPtr<IDxcCompiler3> compiler = nullptr;
		ArcPtr<IDxcUtils> utils = nullptr;
		ArcPtr<IDxcIncludeHandler> include_handler = nullptr;
	}
	class CustomIncludeHandler : public IDxcIncludeHandler
	{
	public:
		CustomIncludeHandler() {}

		HRESULT STDMETHODCALLTYPE LoadSource(_In_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
		{
			ArcPtr<IDxcBlobEncoding> encoding;
			std::string include_file = NormalizePath(ToString(pFilename));
			if (!FileExists(include_file))
			{
				*ppIncludeSource = nullptr;
				return E_FAIL;
			}

			bool already_included = false;
			for (auto const& included_file : include_files)
			{
				if (include_file == included_file)
				{
					already_included = true;
					break;
				}
			}

			if (already_included)
			{
				static const char nullStr[] = " ";
				utils->CreateBlob(nullStr, ARRAYSIZE(nullStr), CP_UTF8, encoding.GetAddressOf());
				*ppIncludeSource = encoding.Detach();
				return S_OK;
			}

			std::wstring winclude_file = ToWideString(include_file);
			HRESULT hr = utils->LoadFile(winclude_file.c_str(), nullptr, encoding.GetAddressOf());
			if (SUCCEEDED(hr))
			{
				include_files.push_back(include_file);
				*ppIncludeSource = encoding.Detach();
				return S_OK;
			}
			else *ppIncludeSource = nullptr;
			return E_FAIL;
		}
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override
		{
			return include_handler->QueryInterface(riid, ppvObject);
		}

		ULONG STDMETHODCALLTYPE AddRef(void) override { return 1; }
		ULONG STDMETHODCALLTYPE Release(void) override { return 1; }

		std::vector<std::string> include_files;
	};
	class ReflectionBlob : public IDxcBlob
	{
	public:
		ReflectionBlob(void const* pShaderBytecode, SIZE_T byteLength) : bytecodeSize{ byteLength }
		{
			pBytecode = const_cast<void*>(pShaderBytecode);
		}
		virtual ~ReflectionBlob() { /*non owning blob -> empty destructor*/ }
		virtual LPVOID STDMETHODCALLTYPE GetBufferPointer(void) override { return pBytecode; }
		virtual SIZE_T STDMETHODCALLTYPE GetBufferSize(void) override { return bytecodeSize; }
		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppv) override
		{
			if (ppv == NULL) return E_POINTER;
			if (riid == __uuidof(IDxcBlob)) //uuid(guid_str)
				*ppv = static_cast<IDxcBlob*>(this);
			else if (riid == IID_IUnknown)
				*ppv = static_cast<IUnknown*>(this);
			else
			{ // unsupported interface
				*ppv = NULL;
				return E_NOINTERFACE;
			}

			reinterpret_cast<IUnknown*>(*ppv)->AddRef();
			return S_OK;

		}
		virtual ULONG STDMETHODCALLTYPE AddRef(void) override { return 1; }
		virtual ULONG STDMETHODCALLTYPE Release(void) override { return 1; }
	private:
		LPVOID pBytecode = nullptr;
		SIZE_T bytecodeSize = 0;
	};

	static char const* shaders_cache_directory = "Resources/ShaderCache/";
	extern char const* shaders_directory;

	inline constexpr std::wstring GetTarget(GfxShaderStage stage, GfxShaderModel model)
	{
		std::wstring target = L"";
		switch (stage)
		{
		case GfxShaderStage::VS:
			target += L"vs_";
			break;
		case GfxShaderStage::PS:
			target += L"ps_";
			break;
		case GfxShaderStage::CS:
			target += L"cs_";
			break;
		case GfxShaderStage::GS:
			target += L"gs_";
			break;
		case GfxShaderStage::HS:
			target += L"hs_";
			break;
		case GfxShaderStage::DS:
			target += L"ds_";
			break;
		case GfxShaderStage::LIB:
			target += L"lib_";
			break;
		case GfxShaderStage::MS:
			target += L"ms_";
			break;
		case GfxShaderStage::AS:
			target += L"as_";
			break;
		default:
			ADRIA_ASSERT(false && "Invalid Shader Stage");
		}
		switch (model)
		{
		case SM_6_0:
			target += L"6_0";
			break;
		case SM_6_1:
			target += L"6_1";
			break;
		case SM_6_2:
			target += L"6_2";
			break;
		case SM_6_3:
			target += L"6_3";
			break;
		case SM_6_4:
			target += L"6_4";
			break;
		case SM_6_5:
			target += L"6_5";
			break;
		case SM_6_6:
			target += L"6_6";
			break;
		default:
			break;
		}
		return target;
	}

	namespace GfxShaderCompiler
	{
		static bool CheckCache(char const* cache_path, GfxShaderCompileInput const& input, GfxShaderCompileOutput& output)
		{
			if (!FileExists(cache_path)) return false;
			if (GetFileLastWriteTime(cache_path) < GetFileLastWriteTime(input.file)) return false;

			std::ifstream is(cache_path, std::ios::binary);
			cereal::BinaryInputArchive archive(is);

			archive(output.shader_hash);
			archive(output.includes);
			size_t binary_size = 0;
			archive(binary_size);
			std::unique_ptr<char[]> binary_data(new char[binary_size]);
			archive.loadBinary(binary_data.get(), binary_size);
			output.shader.SetBytecode(binary_data.get(), binary_size);
			output.shader.SetDesc(input);
			return true;
		}
		static bool SaveToCache(char const* cache_path, GfxShaderCompileOutput const& output)
		{
			std::ofstream os(cache_path, std::ios::binary);
			cereal::BinaryOutputArchive archive(os);
			archive(output.shader_hash);
			archive(output.includes); 
			archive(output.shader.GetLength());
			archive.saveBinary(output.shader.GetPointer(), output.shader.GetLength());
			return true;
		}

		void Initialize()
		{
			BREAK_IF_FAILED(DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(library.GetAddressOf())));
			BREAK_IF_FAILED(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(compiler.GetAddressOf())));
			BREAK_IF_FAILED(library->CreateIncludeHandler(include_handler.GetAddressOf()));
			BREAK_IF_FAILED(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(utils.GetAddressOf())));
		}
		void Destroy()
		{
			include_handler.Reset();
			compiler.Reset();
			library.Reset();
			utils.Reset();
		}
		bool CompileShader(GfxShaderCompileInput const& input, GfxShaderCompileOutput& output)
		{
			std::string macro_key;
			for (GfxShaderMacro const& macro : input.macros)
			{
				macro_key += macro.name;
				macro_key += macro.value;
			}
			uint64 macro_hash = crc64(macro_key.c_str(), macro_key.size());
			
			char cache_path[256];
			sprintf_s(cache_path, "%s%s_%s_%llx.bin", shaders_cache_directory, GetFilenameWithoutExtension(input.file).c_str(), 
												    input.entry_point.c_str(), macro_hash);

			if (CheckCache(cache_path, input, output)) return true;
			ADRIA_LOG(INFO, "Shader '%s.%s' not found in cache. Compiling...", input.file.c_str(), input.entry_point.c_str());

			uint32_t code_page = CP_UTF8; 
			ArcPtr<IDxcBlobEncoding> source_blob;

			std::wstring shader_source = ToWideString(input.file);
			HRESULT hr = library->CreateBlobFromFile(shader_source.data(), &code_page, source_blob.GetAddressOf());
			BREAK_IF_FAILED(hr);

			std::wstring name = ToWideString(GetFilenameWithoutExtension(input.file));
			std::wstring dir  = ToWideString(shaders_directory);
			std::wstring path = ToWideString(GetParentPath(input.file));
			
			std::wstring target = GetTarget(input.stage, input.model);
			std::wstring entry_point = ToWideString(input.entry_point);
			if (entry_point.empty()) entry_point = L"main";

			std::vector<wchar_t const*> compile_args{};
			compile_args.push_back(name.c_str());
			if (input.flags & ShaderCompilerFlag_Debug)
			{
				compile_args.push_back(DXC_ARG_DEBUG);
				compile_args.push_back(L"-Qembed_debug");
			}
			else
			{
				compile_args.push_back(L"-Qstrip_debug");
				compile_args.push_back(L"-Qstrip_reflect");
			}

			if (input.flags & ShaderCompilerFlag_DisableOptimization)
			{
				compile_args.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);
			}
			else
			{
				compile_args.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
			}
			compile_args.push_back(L"-HV 2021");

			compile_args.push_back(L"-E");
			compile_args.push_back(entry_point.c_str());
			compile_args.push_back(L"-T");
			compile_args.push_back(target.c_str());

			compile_args.push_back(L"-I");
			compile_args.push_back(dir.c_str());
			compile_args.push_back(L"-I");
			compile_args.push_back(path.c_str());

			std::vector<std::wstring> macros;
			macros.reserve(input.macros.size());
			for (auto const& macro : input.macros)
			{
				std::wstring macro_name = ToWideString(macro.name);
				std::wstring macro_value = ToWideString(macro.value);
				compile_args.push_back(L"-D");
				if (macro.value.empty()) 
					macros.push_back(macro_name + L"=1");
				else 
					macros.push_back(macro_name + L"=" + macro_value);
				compile_args.push_back(macros.back().c_str());
			}

			DxcBuffer source_buffer;
			source_buffer.Ptr = source_blob->GetBufferPointer();
			source_buffer.Size = source_blob->GetBufferSize();
			source_buffer.Encoding = 0;
			CustomIncludeHandler custom_include_handler{};
			
			ArcPtr<IDxcResult> result;
			hr = compiler->Compile(
				&source_buffer,
				compile_args.data(), (uint32)compile_args.size(),
				&custom_include_handler,
				IID_PPV_ARGS(result.GetAddressOf()));

			ArcPtr<IDxcBlobUtf8> errors;
			if (SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr)))
			{
				if (errors && errors->GetStringLength() > 0)
				{
					ADRIA_LOG(ERROR, "%s", errors->GetStringPointer());
					return false;
				}
			}
			ArcPtr<IDxcBlob> blob;
			BREAK_IF_FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(blob.GetAddressOf()), nullptr));
			ArcPtr<IDxcBlob> hash;
			if (SUCCEEDED(result->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(hash.GetAddressOf()), nullptr)))
			{
				DxcShaderHash* hash_buf = (DxcShaderHash*)hash->GetBufferPointer();
				memcpy(output.shader_hash, hash_buf->HashDigest, sizeof(uint64) * 2);
			}
			
			output.shader.SetDesc(input);
			output.shader.SetBytecode(blob->GetBufferPointer(), blob->GetBufferSize());
			output.includes = std::move(custom_include_handler.include_files);
			output.includes.push_back(input.file);
			SaveToCache(cache_path, output);
			return true;
		}
		void ReadBlobFromFile(std::wstring_view filename, GfxShaderBlob& blob)
		{
			uint32_t code_page = CP_UTF8;
			ArcPtr<IDxcBlobEncoding> source_blob;
			HRESULT hr = library->CreateBlobFromFile(filename.data(), &code_page, source_blob.GetAddressOf());
			BREAK_IF_FAILED(hr);
			blob.resize(source_blob->GetBufferSize());
			memcpy(blob.data(), source_blob->GetBufferPointer(), source_blob->GetBufferSize());
		}
		void CreateInputLayout(GfxShader const& vs_blob, GfxInputLayout& input_layout)
		{
			ArcPtr<IDxcContainerReflection> reflection;
			HRESULT hr = DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(reflection.GetAddressOf()));
			ReflectionBlob my_blob{ vs_blob.GetPointer(), vs_blob.GetLength() };
			BREAK_IF_FAILED(hr);
			hr = reflection->Load(&my_blob);
			BREAK_IF_FAILED(hr);
			uint32_t part_index;
#ifndef MAKEFOURCC
#define MAKEFOURCC(a, b, c, d) (unsigned int)((unsigned char)(a) | (unsigned char)(b) << 8 | (unsigned char)(c) << 16 | (unsigned char)(d) << 24)
#endif
			BREAK_IF_FAILED(reflection->FindFirstPartKind(MAKEFOURCC('D', 'X', 'I', 'L'), &part_index));
#undef MAKEFOURCC

			ArcPtr<ID3D12ShaderReflection> vertex_shader_reflection;
			BREAK_IF_FAILED(reflection->GetPartReflection(part_index, IID_PPV_ARGS(vertex_shader_reflection.GetAddressOf())));

			D3D12_SHADER_DESC shader_desc;
			BREAK_IF_FAILED(vertex_shader_reflection->GetDesc(&shader_desc));

			D3D12_SIGNATURE_PARAMETER_DESC param_desc{};
			D3D12_INPUT_ELEMENT_DESC d3d12element_desc{};

			input_layout.elements.clear();
			input_layout.elements.resize(shader_desc.InputParameters);
			for (uint32_t i = 0; i < shader_desc.InputParameters; i++)
			{
				vertex_shader_reflection->GetInputParameterDesc(i, &param_desc);
				input_layout.elements[i].semantic_name = std::string(param_desc.SemanticName);
				input_layout.elements[i].semantic_index = param_desc.SemanticIndex;
				input_layout.elements[i].aligned_byte_offset = GfxInputLayout::APPEND_ALIGNED_ELEMENT;
				input_layout.elements[i].input_slot_class = GfxInputClassification::PerVertexData;
				input_layout.elements[i].input_slot = 0;

				// determine DXGI format
				if (param_desc.Mask == 1)
				{
					if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)		 input_layout.elements[i].format = GfxFormat::R32_UINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.elements[i].format = GfxFormat::R32_SINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.elements[i].format = GfxFormat::R32_FLOAT;
				}
				else if (param_desc.Mask <= 3)
				{
					if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)		 input_layout.elements[i].format = GfxFormat::R32G32_UINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.elements[i].format = GfxFormat::R32G32_SINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.elements[i].format = GfxFormat::R32G32_FLOAT;
				}
				else if (param_desc.Mask <= 7)
				{
					if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)		 input_layout.elements[i].format = GfxFormat::R32G32B32_UINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.elements[i].format = GfxFormat::R32G32B32_SINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.elements[i].format = GfxFormat::R32G32B32_FLOAT;
				}
				else if (param_desc.Mask <= 15)
				{
					if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)		 input_layout.elements[i].format = GfxFormat::R32G32B32A32_UINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.elements[i].format = GfxFormat::R32G32B32A32_SINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.elements[i].format = GfxFormat::R32G32B32A32_FLOAT;
				}
			}
		}
	}
}