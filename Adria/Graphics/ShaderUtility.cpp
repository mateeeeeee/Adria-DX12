#pragma comment(lib, "dxcompiler.lib")
#include "ShaderUtility.h"
#include <wrl.h>
#include <d3dcompiler.h> 
#include "dxc/dxcapi.h" 
#include "../Utilities/StringUtil.h"
#include "../Logging/Logger.h"


namespace adria
{

	//for shader reflection
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

		virtual ULONG STDMETHODCALLTYPE AddRef(void) override {return 1;}

		virtual ULONG STDMETHODCALLTYPE Release(void) override { return 1; }
	private:
		LPVOID pBytecode = nullptr;
		SIZE_T bytecodeSize = 0;
	};

	namespace
	{
		Microsoft::WRL::ComPtr<IDxcLibrary> library = nullptr;
		Microsoft::WRL::ComPtr<IDxcCompiler> compiler = nullptr;
		Microsoft::WRL::ComPtr<IDxcIncludeHandler> include_handler = nullptr;
	}

	namespace ShaderUtility
	{
		void Initialize()
		{
			HRESULT hr = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
			BREAK_IF_FAILED(hr);
			hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
			BREAK_IF_FAILED(hr);
			BREAK_IF_FAILED(library->CreateIncludeHandler(&include_handler));

		}
		void Destroy()
		{
			include_handler.Reset();
			compiler.Reset();
			library.Reset();
		}
		void CompileShader(ShaderInfo const& input, ShaderBlob& blob)
		{
			Microsoft::WRL::ComPtr<IDxcBlob> _blob;
			uint32_t codePage = CP_UTF8; 
			Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;

			std::wstring shader_source = ConvertToWide(input.shadersource);

			HRESULT hr = library->CreateBlobFromFile(shader_source.data(), &codePage, &sourceBlob);
			BREAK_IF_FAILED(hr);

			std::vector<wchar_t const*> flags{};
			if (input.flags & ShaderInfo::FLAG_DEBUG)
			{
				flags.push_back(L"-Zi");			//Debug info
				flags.push_back(L"-Qembed_debug");	//Embed debug info into the shader
			}
			if (input.flags & ShaderInfo::FLAG_DISABLE_OPTIMIZATION)
				flags.push_back(L"-Od");
			else flags.push_back(L"-O3");

			std::wstring p_target = L"";
			std::wstring entry_point = L"";

			switch (input.stage)
			{
			case EShaderStage::VS:
				p_target = L"vs_6_0";
				entry_point = L"vs_main";
				break;
			case EShaderStage::PS:
				p_target = L"ps_6_0";
				entry_point = L"ps_main";
				break;
			case EShaderStage::CS:
				p_target = L"cs_6_0";
				entry_point = L"cs_main";
				break;
			case EShaderStage::GS:
				p_target = L"gs_6_0";
				entry_point = L"gs_main";
				break;
			case EShaderStage::HS:
				p_target = L"hs_6_0";
				entry_point = L"hs_main";
				break;
			case EShaderStage::DS:
				p_target = L"ds_6_0";
				entry_point = L"ds_main";
				break;
			case EShaderStage::LIB:
				p_target = L"lib_6_3";
				break;
			default:
				ADRIA_ASSERT(false && "Invalid Shader Stage");
			}

			if (!input.entrypoint.empty()) entry_point = ConvertToWide(input.entrypoint);

			std::vector<DxcDefine> sm6_defines{};
			for (auto const& define : input.defines)
			{
				DxcDefine sm6_define{};
				sm6_define.Name = define.name.c_str();
				sm6_define.Value = define.value.c_str();
				sm6_defines.push_back(sm6_define);
			}

			Microsoft::WRL::ComPtr<IDxcOperationResult> result;
			hr = compiler->Compile(
				sourceBlob.Get(),									// pSource
				shader_source.data(),								// pSourceName
				entry_point.c_str(),								// pEntryPoint
				p_target.c_str(),									// pTargetProfile
				flags.data(), (UINT32)flags.size(),					// pArguments, argCount
				sm6_defines.data(), (UINT32)sm6_defines.size(),	// pDefines, defineCount
				include_handler.Get(),					// pIncludeHandler
				&result);								// ppResult

			if (SUCCEEDED(hr))
				result->GetStatus(&hr);

			if (FAILED(hr) && result)
			{
				Microsoft::WRL::ComPtr<IDxcBlobEncoding> errorsBlob;
				hr = result->GetErrorBuffer(&errorsBlob);
				if (SUCCEEDED(hr) && errorsBlob)
					ADRIA_LOG(ERROR, "Compilation failed with errors:\n%hs\n", (const char*)errorsBlob->GetBufferPointer());
			}

			result->GetResult(_blob.GetAddressOf());
			blob.bytecode.resize(_blob->GetBufferSize());
			memcpy(blob.GetPointer(), _blob->GetBufferPointer(), blob.GetLength());
		}
		void GetBlobFromCompiledShader(std::wstring_view filename, ShaderBlob& blob)
		{
			uint32_t codePage = CP_UTF8;
			Microsoft::WRL::ComPtr<IDxcBlobEncoding> sourceBlob;
			HRESULT hr = library->CreateBlobFromFile(filename.data(), &codePage, &sourceBlob);
			BREAK_IF_FAILED(hr);

			blob.bytecode.resize(sourceBlob->GetBufferSize());
			memcpy(blob.GetPointer(), sourceBlob->GetBufferPointer(), blob.GetLength());
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

			// Read input layout description from shader info
			D3D12_SIGNATURE_PARAMETER_DESC param_desc{};
			D3D12_INPUT_ELEMENT_DESC d3d12element_desc{};

			input_layout.il_desc.clear();
			input_layout.semantic_names.clear();

			input_layout.il_desc.resize(shaderDesc.InputParameters);

			for (uint32_t i = 0; i < shaderDesc.InputParameters; i++)
			{

				pVertexShaderReflection->GetInputParameterDesc(i, &param_desc);

				input_layout.semantic_names.push_back(param_desc.SemanticName);

				input_layout.il_desc[i].SemanticName = input_layout.semantic_names.back().c_str();
				input_layout.il_desc[i].SemanticIndex = param_desc.SemanticIndex;
				input_layout.il_desc[i].InputSlot = 0;
				input_layout.il_desc[i].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
				input_layout.il_desc[i].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
				input_layout.il_desc[i].InstanceDataStepRate = 0;

				// determine DXGI format
				if (param_desc.Mask == 1)
				{
					if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)		 input_layout.il_desc[i].Format = DXGI_FORMAT_R32_UINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.il_desc[i].Format = DXGI_FORMAT_R32_SINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.il_desc[i].Format = DXGI_FORMAT_R32_FLOAT;
				}
				else if (param_desc.Mask <= 3)
				{
					if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)		 input_layout.il_desc[i].Format = DXGI_FORMAT_R32G32_UINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.il_desc[i].Format = DXGI_FORMAT_R32G32_SINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.il_desc[i].Format = DXGI_FORMAT_R32G32_FLOAT;
				}
				else if (param_desc.Mask <= 7)
				{
					if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)		 input_layout.il_desc[i].Format = DXGI_FORMAT_R32G32B32_UINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.il_desc[i].Format = DXGI_FORMAT_R32G32B32_SINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.il_desc[i].Format = DXGI_FORMAT_R32G32B32_FLOAT;
				}
				else if (param_desc.Mask <= 15)
				{
					if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)		 input_layout.il_desc[i].Format = DXGI_FORMAT_R32G32B32A32_UINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.il_desc[i].Format = DXGI_FORMAT_R32G32B32A32_SINT;
					else if (param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.il_desc[i].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				}
			}

		}
	}
}