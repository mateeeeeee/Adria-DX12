#pragma once
#include <array>
#include <vector>
#include <d3d12.h>
#include <DirectXMath.h>


namespace adria
{

	constexpr uint16 NUMBER_OF_LAYOUTS = 7U;

	static std::vector<D3D12_INPUT_ELEMENT_DESC> Layouts[NUMBER_OF_LAYOUTS] =
	{
		/*1.*/ {{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }},
		/*2.*/ {{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },{ "TEX", 0, DXGI_FORMAT_R32G32_FLOAT,0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }},
		/*3.*/ {{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },{ "TEX", 0, DXGI_FORMAT_R32G32_FLOAT,0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }},
		/*4.*/ {{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }},
		/*5.*/ {{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }},
		/*6.*/ {{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }},
		/*7.*/ {{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },{ "TEX", 0, DXGI_FORMAT_R32G32_FLOAT,0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT,0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }}
	};


	struct SimpleVertex
	{
		Vector3 position;

		static std::vector<D3D12_INPUT_ELEMENT_DESC> GetLayout()
		{
			return Layouts[0u];
		}

		static constexpr UINT GetComponentsNumbers()
		{
			return 1u;
		}
	};

	struct TexturedVertex
	{
		Vector3 position;
		Vector2 uv;

		static std::vector<D3D12_INPUT_ELEMENT_DESC> GetLayout()
		{
			return Layouts[1u];
		}

		static constexpr UINT GetComponentsNumbers()
		{
			return 2u;
		}
	};

	struct TexturedNormalVertex
	{
		Vector3 position;
		Vector2 uv;
		Vector3 normal;

		static std::vector<D3D12_INPUT_ELEMENT_DESC> GetLayout()
		{
			return Layouts[2u];
		}

		static constexpr UINT GetComponentsNumbers()
		{
			return 3u;
		}
	};

	struct ColoredVertex
	{
		Vector3 position;
		Vector4 color;

		static std::vector<D3D12_INPUT_ELEMENT_DESC> GetLayout()
		{
			return Layouts[3u];
		}

		static constexpr UINT GetComponentsNumbers()
		{
			return 2u;
		}
	};

	struct ColoredNormalVertex
	{
		Vector3 position;
		Vector4 color;
		Vector3 normal;

		static std::vector<D3D12_INPUT_ELEMENT_DESC> GetLayout()
		{
			return Layouts[4u];
		}

		static constexpr UINT GetComponentsNumbers()
		{
			return 3u;
		}
	};

	struct NormalVertex
	{
		Vector3 position;
		Vector3 normal;

		static std::vector<D3D12_INPUT_ELEMENT_DESC> GetLayout()
		{
			return Layouts[5u];
		}

		static constexpr UINT GetComponentsNumbers()
		{
			return 2u;
		}
	};

	struct CompleteVertex
	{
		Vector3 position;
		Vector2 uv;
		Vector3 normal;
		Vector4 tangent;

		static std::vector<D3D12_INPUT_ELEMENT_DESC> GetLayout()
		{
			return Layouts[6u];
		}

		static constexpr UINT GetComponentsNumbers()
		{
			return 4u;
		}
	};

}

