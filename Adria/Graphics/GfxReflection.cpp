#include <d3dcompiler.h>
#include "dxcapi.h"
#include "GfxMacros.h"
#include "GfxReflection.h"
#include "GfxShader.h"
#include "GfxInputLayout.h"

namespace adria
{
	class GfxReflectionBlob : public IDxcBlob
	{
	public:
		GfxReflectionBlob(void const* pShaderBytecode, Uint64 byteLength) : bytecodeSize{ byteLength }
		{
			pBytecode = const_cast<void*>(pShaderBytecode);
		}
		virtual ~GfxReflectionBlob() { /*non owning blob -> empty destructor*/ }
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
			{
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

	void GfxReflection::FillInputLayoutDesc(GfxShader const& vertex_shader, GfxInputLayout& input_layout)
	{
		Ref<IDxcContainerReflection> reflection;
		HRESULT hr = DxcCreateInstance(CLSID_DxcContainerReflection, IID_PPV_ARGS(reflection.GetAddressOf()));
		GfxReflectionBlob my_blob{ vertex_shader.GetData(), vertex_shader.GetSize() };
		GFX_CHECK_HR(hr);
		hr = reflection->Load(&my_blob);
		GFX_CHECK_HR(hr);
		uint32_t part_index;
#ifndef MAKEFOURCC
#define MAKEFOURCC(a, b, c, d) (Uint)((Uchar)(a) | (Uchar)(b) << 8 | (Uchar)(c) << 16 | (Uchar)(d) << 24)
#endif
		GFX_CHECK_HR(reflection->FindFirstPartKind(MAKEFOURCC('D', 'X', 'I', 'L'), &part_index));
#undef MAKEFOURCC

		Ref<ID3D12ShaderReflection> vertex_shader_reflection;
		GFX_CHECK_HR(reflection->GetPartReflection(part_index, IID_PPV_ARGS(vertex_shader_reflection.GetAddressOf())));

		D3D12_SHADER_DESC shader_desc;
		GFX_CHECK_HR(vertex_shader_reflection->GetDesc(&shader_desc));

		D3D12_SIGNATURE_PARAMETER_DESC signature_param_desc{};
		input_layout.elements.clear();
		input_layout.elements.resize(shader_desc.InputParameters);
		for (Uint32 i = 0; i < shader_desc.InputParameters; i++)
		{
			vertex_shader_reflection->GetInputParameterDesc(i, &signature_param_desc);
			input_layout.elements[i].semantic_name = std::string(signature_param_desc.SemanticName);
			input_layout.elements[i].semantic_index = signature_param_desc.SemanticIndex;
			input_layout.elements[i].aligned_byte_offset = GfxInputLayout::APPEND_ALIGNED_ELEMENT;
			input_layout.elements[i].input_slot_class = GfxInputClassification::PerVertexData;
			input_layout.elements[i].input_slot = 0;

			if (input_layout.elements[i].semantic_name.starts_with("INSTANCE"))
			{
				input_layout.elements[i].input_slot_class = GfxInputClassification::PerInstanceData;
				input_layout.elements[i].input_slot = 1;
			}

			if (signature_param_desc.Mask == 1)
			{
				if (signature_param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)	   input_layout.elements[i].format = GfxFormat::R32_UINT;
				else if (signature_param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.elements[i].format = GfxFormat::R32_SINT;
				else if (signature_param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.elements[i].format = GfxFormat::R32_FLOAT;
			}
			else if (signature_param_desc.Mask <= 3)
			{
				if (signature_param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)	   input_layout.elements[i].format = GfxFormat::R32G32_UINT;
				else if (signature_param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.elements[i].format = GfxFormat::R32G32_SINT;
				else if (signature_param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.elements[i].format = GfxFormat::R32G32_FLOAT;
			}
			else if (signature_param_desc.Mask <= 7)
			{
				if (signature_param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)	   input_layout.elements[i].format = GfxFormat::R32G32B32_UINT;
				else if (signature_param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.elements[i].format = GfxFormat::R32G32B32_SINT;
				else if (signature_param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.elements[i].format = GfxFormat::R32G32B32_FLOAT;
			}
			else if (signature_param_desc.Mask <= 15)
			{
				if (signature_param_desc.ComponentType == D3D_REGISTER_COMPONENT_UINT32)	   input_layout.elements[i].format = GfxFormat::R32G32B32A32_UINT;
				else if (signature_param_desc.ComponentType == D3D_REGISTER_COMPONENT_SINT32)  input_layout.elements[i].format = GfxFormat::R32G32B32A32_SINT;
				else if (signature_param_desc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) input_layout.elements[i].format = GfxFormat::R32G32B32A32_FLOAT;
			}
		}
	}

}

