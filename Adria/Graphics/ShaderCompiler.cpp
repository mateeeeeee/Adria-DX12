#pragma comment(lib, "dxcompiler.lib")
#include "ShaderCompiler.h"
#include <wrl.h>
#include <d3dcompiler.h> 
#include "dxc/dxcapi.h" 
#include "../Utilities/StringUtil.h"
#include "../Utilities/FilesUtil.h"
#include "../Logging/Logger.h"


namespace adria
{
	extern char const* shaders_directory;
	namespace
	{
		Microsoft::WRL::ComPtr<IDxcLibrary> library = nullptr;
		Microsoft::WRL::ComPtr<IDxcCompiler3> compiler = nullptr;
		Microsoft::WRL::ComPtr<IDxcUtils> utils = nullptr;
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> include_handler = nullptr;
	}

	class CustomIncludeHandler : public IDxcIncludeHandler
	{
	public:
		CustomIncludeHandler() {}

		HRESULT STDMETHODCALLTYPE LoadSource(_In_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
		{
			Microsoft::WRL::ComPtr<IDxcBlobEncoding> encoding;
			std::string include_file = NormalizePath(ToString(pFilename));
			if (!FileExists(include_file))
			{
				*ppIncludeSource = nullptr;
				return E_FAIL;
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

	namespace ShaderCompiler
	{
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
		void CompileShader(ShaderCompileInput const& input, ShaderCompileOutput& output)
		{
			uint32_t code_page = CP_UTF8; 
			Microsoft::WRL::ComPtr<IDxcBlobEncoding> source_blob;

			std::wstring shader_source = ToWideString(input.source_file);
			HRESULT hr = library->CreateBlobFromFile(shader_source.data(), &code_page, &source_blob);
			BREAK_IF_FAILED(hr);

			std::wstring name = ToWideString(GetFilenameWithoutExtension(input.source_file));
			std::wstring dir = ToWideString(shaders_directory);
			std::wstring path = ToWideString(GetParentPath(input.source_file));
			
			std::wstring p_target = L"";
			std::wstring entry_point = L"";
			switch (input.stage)
			{
			case EShaderStage::VS:
				p_target = L"vs_6_5";
				entry_point = L"vs_main";
				break;
			case EShaderStage::PS:
				p_target = L"ps_6_5";
				entry_point = L"ps_main";
				break;
			case EShaderStage::CS:
				p_target = L"cs_6_5";
				entry_point = L"cs_main";
				break;
			case EShaderStage::GS:
				p_target = L"gs_6_5";
				entry_point = L"gs_main";
				break;
			case EShaderStage::HS:
				p_target = L"hs_6_5";
				entry_point = L"hs_main";
				break;
			case EShaderStage::DS:
				p_target = L"ds_6_5";
				entry_point = L"ds_main";
				break;
			case EShaderStage::LIB:
				p_target = L"lib_6_5";
				break;
			default:
				ADRIA_ASSERT(false && "Invalid Shader Stage");
			}

			if (!input.entrypoint.empty()) entry_point = ToWideString(input.entrypoint);

			std::vector<wchar_t const*> compile_args{};
			compile_args.push_back(name.c_str());
			if (input.flags & ShaderCompileInput::FlagDebug)
			{
				compile_args.push_back(DXC_ARG_DEBUG);
				compile_args.push_back(L"-Qembed_debug");
			}
			else
			{
				compile_args.push_back(L"-Qstrip_debug");
				compile_args.push_back(L"-Qstrip_reflect");
			}

			if (input.flags & ShaderCompileInput::FlagDisableOptimization)
			{
				compile_args.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);
			}
			else
			{
				compile_args.push_back(DXC_ARG_OPTIMIZATION_LEVEL3);
			}

			compile_args.push_back(L"-E");
			compile_args.push_back(entry_point.c_str());
			compile_args.push_back(L"-T");
			compile_args.push_back(p_target.c_str());

			compile_args.push_back(L"-I");
			compile_args.push_back(dir.c_str());
			compile_args.push_back(L"-I");
			compile_args.push_back(path.c_str());

			std::vector<std::wstring> macros;
			macros.reserve(input.macros.size());
			for (auto const& define : input.macros)
			{
				compile_args.push_back(L"-D");
				if (define.value.empty()) macros.push_back(define.name + L"=1");
				else macros.push_back(define.name + L"=" + define.value);
				compile_args.push_back(macros.back().c_str());
			}

			CustomIncludeHandler custom_include_handler{};

			DxcBuffer source_buffer;
			source_buffer.Ptr = source_blob->GetBufferPointer();
			source_buffer.Size = source_blob->GetBufferSize();
			source_buffer.Encoding = 0;
			Microsoft::WRL::ComPtr<IDxcResult> result;
			hr = compiler->Compile(
				&source_buffer,										// buffer 
				compile_args.data(), (uint32)compile_args.size(),	//args
				&custom_include_handler,							// pIncludeHandler
				IID_PPV_ARGS(result.GetAddressOf()));				// ppResult

			Microsoft::WRL::ComPtr<IDxcBlobUtf8> errors;
			if (SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(errors.GetAddressOf()), nullptr)))
			{
				if (errors && errors->GetStringLength() > 0)
				{
					std::string a = errors->GetStringPointer();
					ADRIA_LOG(DEBUG, "%s", errors->GetStringPointer());
					return;
				}
			}
			Microsoft::WRL::ComPtr<IDxcBlob> _blob;
			BREAK_IF_FAILED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(_blob.GetAddressOf()), nullptr));
			if (_blob->GetBufferSize() == 0)
			{
				int x = 5;
			}
			output.blob.bytecode.resize(_blob->GetBufferSize());
			memcpy(output.blob.GetPointer(), _blob->GetBufferPointer(), _blob->GetBufferSize());
			output.dependent_files = custom_include_handler.include_files;
			output.dependent_files.push_back(input.source_file);
		}
		void GetBlobFromCompiledShader(std::wstring_view filename, ShaderBlob& blob)
		{
			uint32_t codePage = CP_UTF8;
			Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
			HRESULT hr = library->CreateBlobFromFile(filename.data(), &codePage, &sourceBlob);
			BREAK_IF_FAILED(hr);

			blob.bytecode.resize(sourceBlob->GetBufferSize());
			memcpy(blob.GetPointer(), sourceBlob->GetBufferPointer(), sourceBlob->GetBufferSize());
		}
		void CreateInputLayoutWithReflection(ShaderBlob const& vs_blob, InputLayout& input_layout)
		{
			Microsoft::WRL::ComPtr<IDxcContainerReflection> pReflection;
			HRESULT hr = DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(pReflection.GetAddressOf()));
			ReflectionBlob my_blob{ vs_blob.GetPointer() , vs_blob.GetLength() };
			BREAK_IF_FAILED(hr);
			hr = pReflection->Load(&my_blob);
			BREAK_IF_FAILED(hr);
			uint32_t partIndex;
#ifndef MAKEFOURCC
#define MAKEFOURCC(a, b, c, d) (unsigned int)((unsigned char)(a) | (unsigned char)(b) << 8 | (unsigned char)(c) << 16 | (unsigned char)(d) << 24)
#endif
			BREAK_IF_FAILED(pReflection->FindFirstPartKind(MAKEFOURCC('D', 'X', 'I', 'L'), &partIndex));
#undef MAKEFOURCC

			Microsoft::WRL::ComPtr<ID3D12ShaderReflection> pVertexShaderReflection;
			BREAK_IF_FAILED(pReflection->GetPartReflection(partIndex, IID_PPV_ARGS(pVertexShaderReflection.GetAddressOf())));

			D3D12_SHADER_DESC shaderDesc;
			BREAK_IF_FAILED(pVertexShaderReflection->GetDesc(&shaderDesc));

			D3D12_SIGNATURE_PARAMETER_DESC param_desc{};
			D3D12_INPUT_ELEMENT_DESC d3d12element_desc{};

			input_layout.elements.clear();
			input_layout.elements.resize(shaderDesc.InputParameters);
			for (uint32_t i = 0; i < shaderDesc.InputParameters; i++)
			{
				pVertexShaderReflection->GetInputParameterDesc(i, &param_desc);
				input_layout.elements[i].semantic_name = std::string(param_desc.SemanticName);
				input_layout.elements[i].semantic_index = param_desc.SemanticIndex;
				input_layout.elements[i].aligned_byte_offset = InputLayout::APPEND_ALIGNED_ELEMENT;
				input_layout.elements[i].input_slot_class = EInputClassification::PerVertexData;
				input_layout.elements[i].input_slot = 0;

				// determine DXGI format
				if (param_desc.Mask == 1)
				{
					if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)		 input_layout.elements[i].format = DXGI_FORMAT_R32_UINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.elements[i].format = DXGI_FORMAT_R32_SINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.elements[i].format = DXGI_FORMAT_R32_FLOAT;
				}
				else if (param_desc.Mask <= 3)
				{
					if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)		 input_layout.elements[i].format = DXGI_FORMAT_R32G32_UINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.elements[i].format = DXGI_FORMAT_R32G32_SINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.elements[i].format = DXGI_FORMAT_R32G32_FLOAT;
				}
				else if (param_desc.Mask <= 7)
				{
					if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)		 input_layout.elements[i].format = DXGI_FORMAT_R32G32B32_UINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.elements[i].format = DXGI_FORMAT_R32G32B32_SINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.elements[i].format = DXGI_FORMAT_R32G32B32_FLOAT;
				}
				else if (param_desc.Mask <= 15)
				{
					if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)		 input_layout.elements[i].format = DXGI_FORMAT_R32G32B32A32_UINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.elements[i].format = DXGI_FORMAT_R32G32B32A32_SINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.elements[i].format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				}
			}
		}
	}
}