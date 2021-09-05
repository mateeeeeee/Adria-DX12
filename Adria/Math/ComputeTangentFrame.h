#pragma once
#include <DirectXMath.h>

using namespace DirectX;

namespace adria
{
	void ComputeTangentFrame(
		_In_reads_(nIndices) uint32_t const* indices, size_t nIndices,
		_In_reads_(nVerts) XMFLOAT3 const* positions,
		_In_reads_(nVerts) XMFLOAT3 const* normals,
		_In_reads_(nVerts) XMFLOAT2 const* texcoords,
		size_t nVerts,
		_Out_writes_opt_(nVerts) XMFLOAT3* out_tangents,
		_Out_writes_opt_(nVerts) XMFLOAT3* out_bitangents) noexcept
	{
		XMFLOAT3* tangents = new XMFLOAT3[nVerts]{}; //use unique_ptr<T[]>
		XMFLOAT3* bitangents = new XMFLOAT3[nVerts]{};
		
		for (uint32_t i = 0; i <= nIndices - 3; i += 3)
		{
			uint32_t i0 = indices[i + 0];
			uint32_t i1 = indices[i + 1];
			uint32_t i2 = indices[i + 2];

			XMFLOAT3 p0 = positions[i0];
			XMFLOAT3 p1 = positions[i1];
			XMFLOAT3 p2 = positions[i2];

			XMFLOAT2 w0 = texcoords[i0];
			XMFLOAT2 w1 = texcoords[i1];
			XMFLOAT2 w2 = texcoords[i2];

			XMFLOAT3 e1(p1.x - p0.x, p1.y - p0.y, p1.z - p0.z);
			XMFLOAT3 e2(p2.x - p0.x, p2.y - p0.y, p2.z - p0.z);
			
			float x1 = w1.x - w0.x, x2 = w2.x - w0.x;
			float y1 = w1.y - w0.y, y2 = w2.y - w0.y;
			float r = 1.0F / (x1 * y2 - x2 * y1);

			XMVECTOR _e1 = XMLoadFloat3(&e1);
			XMVECTOR _e2 = XMLoadFloat3(&e2);

			XMVECTOR _t = XMVectorScale(XMVectorSubtract(XMVectorScale(_e1, y2),
				XMVectorScale(_e2, y1)), r);
			XMVECTOR _b = XMVectorScale(XMVectorSubtract(XMVectorScale(_e2, x1),
				XMVectorScale(_e1, x2)), r);

			XMVECTOR _tangent0 = XMLoadFloat3(tangents + i0);
			XMVECTOR _tangent1 = XMLoadFloat3(tangents + i1);
			XMVECTOR _tangent2 = XMLoadFloat3(tangents + i2);

			_tangent0 = XMVectorAdd(_tangent0, _t);
			_tangent1 = XMVectorAdd(_tangent1, _t);
			_tangent2 = XMVectorAdd(_tangent2, _t);

			XMVECTOR _bitangent0 = XMLoadFloat3(bitangents + i0);
			XMVECTOR _bitangent1 = XMLoadFloat3(bitangents + i1);
			XMVECTOR _bitangent2 = XMLoadFloat3(bitangents + i2);

			_bitangent0 = XMVectorAdd(_bitangent0, _b);
			_bitangent1 = XMVectorAdd(_bitangent1, _b);
			_bitangent2 = XMVectorAdd(_bitangent2, _b);

			XMStoreFloat3(tangents + i0, _tangent0);
			XMStoreFloat3(tangents + i1, _tangent1);
			XMStoreFloat3(tangents + i2, _tangent2);
			XMStoreFloat3(bitangents + i0, _bitangent0);
			XMStoreFloat3(bitangents + i1, _bitangent1);
			XMStoreFloat3(bitangents + i2, _bitangent2);
		}

		for (size_t i = 0; i < nVerts; ++i)
		{
			XMFLOAT3 const& n = normals[i];
			XMFLOAT3 const& t = tangents[i];
			XMFLOAT3 const& b = bitangents[i];

			XMVECTOR _out_tangent = XMVector3Normalize(XMVectorSubtract(XMLoadFloat3(&t), XMVector3Dot(XMLoadFloat3(&n), XMLoadFloat3(&t))));
			XMStoreFloat3(out_tangents + i, _out_tangent);
			// Calculate handedness
			float tangent_w = (XMVectorGetX(XMVector3Dot(XMVector3Cross(XMLoadFloat3(&n), XMLoadFloat3(&t)), XMLoadFloat3(&b))) < 0.0F) ? -1.0F : 1.0F;
			XMVECTOR _bitangent = XMVectorScale(XMVector3Cross(XMLoadFloat3(&n), XMLoadFloat3(out_tangents + i)), tangent_w);
			XMStoreFloat3(out_bitangents + i, XMVector3Normalize(_bitangent));
		}

		delete[] tangents;
		delete[] bitangents;
	}
}

