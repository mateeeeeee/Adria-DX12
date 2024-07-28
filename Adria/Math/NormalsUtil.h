#pragma once
#include <DirectXMath.h>
#include <vector>
#include <concepts>


namespace adria
{
    template<typename V>
    concept HasPositionAndNormal = requires (V v)
    {
        {v.position}  -> std::convertible_to<DirectX::XMFLOAT3>;
        {v.normal}    -> std::convertible_to<DirectX::XMFLOAT3>;
    };

    namespace impl
    {
        using namespace DirectX;

        template<typename vertex_t, typename index_t> requires HasPositionAndNormal<vertex_t>&& std::integral<index_t>
            void ComputeNormalsEqualWeight(
                std::vector<vertex_t>& vertices,
                std::vector<index_t> const& indices,
                bool cw = false
        )
        {
            std::vector<XMVECTOR> normals(vertices.size());
            for (uint64 face = 0; face < indices.size() / 3; ++face)
            {
                index_t i0 = indices[face * 3];
                index_t i1 = indices[face * 3 + 1];
                index_t i2 = indices[face * 3 + 2];

                if (i0 == index_t(-1)
                    || i1 == index_t(-1)
                    || i2 == index_t(-1))
                    continue;

                if (i0 >= vertices.size()
                    || i1 >= vertices.size()
                    || i2 >= vertices.size())
                    break;

                XMVECTOR p1 = XMLoadFloat3(&vertices[i0].position);
                XMVECTOR p2 = XMLoadFloat3(&vertices[i1].position);
                XMVECTOR p3 = XMLoadFloat3(&vertices[i2].position);

                XMVECTOR u = XMVectorSubtract(p2, p1);
                XMVECTOR v = XMVectorSubtract(p3, p1);

                XMVECTOR faceNormal = XMVector3Normalize(XMVector3Cross(u, v));

                normals[i0] = XMVectorAdd(normals[i0], faceNormal);
                normals[i1] = XMVectorAdd(normals[i1], faceNormal);
                normals[i2] = XMVectorAdd(normals[i2], faceNormal);
            }

            // Store results
            if (cw)
            {
                for (size_t vert = 0; vert < vertices.size(); ++vert)
                {
                    XMVECTOR n = XMVector3Normalize(normals[vert]);
                    n = XMVectorNegate(n);
                    XMStoreFloat3(&vertices[vert].normal, n);
                }
            }
            else
            {
                for (size_t vert = 0; vert < vertices.size(); ++vert)
                {
                    XMVECTOR n = XMVector3Normalize(normals[vert]);
                    XMStoreFloat3(&vertices[vert].normal, n);
                }
            }


        }


        template<typename vertex_t, typename index_t> requires HasPositionAndNormal<vertex_t>&& std::integral<index_t>
        void ComputeNormalsWeightedByAngle(
            std::vector<vertex_t>& vertices,
            std::vector<index_t> const& indices,
            bool cw = false)
        {
            std::vector<XMVECTOR> normals(vertices.size());

            for (size_t face = 0; face < indices.size() / 3; ++face)
            {
                index_t i0 = indices[face * 3];
                index_t i1 = indices[face * 3 + 1];
                index_t i2 = indices[face * 3 + 2];

                if (i0 == index_t(-1)
                    || i1 == index_t(-1)
                    || i2 == index_t(-1))
                    continue;

                if (i0 >= vertices.size()
                    || i1 >= vertices.size()
                    || i2 >= vertices.size())
                    return;

                XMVECTOR p0 = XMLoadFloat3(&vertices[i0].position);
                XMVECTOR p1 = XMLoadFloat3(&vertices[i1].position);
                XMVECTOR p2 = XMLoadFloat3(&vertices[i2].position);

                XMVECTOR u = XMVectorSubtract(p1, p0);
                XMVECTOR v = XMVectorSubtract(p2, p0);

                XMVECTOR faceNormal = XMVector3Normalize(XMVector3Cross(u, v));

                // Corner 0 -> 1 - 0, 2 - 0
                XMVECTOR a = XMVector3Normalize(u);
                XMVECTOR b = XMVector3Normalize(v);
                XMVECTOR w0 = XMVector3Dot(a, b);
                w0 = XMVectorClamp(w0, g_XMNegativeOne, g_XMOne);
                w0 = XMVectorACos(w0);

                // Corner 1 -> 2 - 1, 0 - 1
                XMVECTOR c = XMVector3Normalize(XMVectorSubtract(p2, p1));
                XMVECTOR d = XMVector3Normalize(XMVectorSubtract(p0, p1));
                XMVECTOR w1 = XMVector3Dot(c, d);
                w1 = XMVectorClamp(w1, g_XMNegativeOne, g_XMOne);
                w1 = XMVectorACos(w1);

                // Corner 2 -> 0 - 2, 1 - 2
                XMVECTOR e = XMVector3Normalize(XMVectorSubtract(p0, p2));
                XMVECTOR f = XMVector3Normalize(XMVectorSubtract(p1, p2));
                XMVECTOR w2 = XMVector3Dot(e, f);
                w2 = XMVectorClamp(w2, g_XMNegativeOne, g_XMOne);
                w2 = XMVectorACos(w2);

                normals[i0] = XMVectorMultiplyAdd(faceNormal, w0, normals[i0]);
                normals[i1] = XMVectorMultiplyAdd(faceNormal, w1, normals[i1]);
                normals[i2] = XMVectorMultiplyAdd(faceNormal, w2, normals[i2]);
            }

            // Store results
            if (cw)
            {
                for (size_t vert = 0; vert < vertices.size(); ++vert)
                {
                    XMVECTOR n = XMVector3Normalize(normals[vert]);
                    n = XMVectorNegate(n);
                    XMStoreFloat3(&vertices[vert].normal, n);
                }
            }
            else
            {
                for (size_t vert = 0; vert < vertices.size(); ++vert)
                {
                    XMVECTOR n = XMVector3Normalize(normals[vert]);
                    XMStoreFloat3(&vertices[vert].normal, n);
                }
            }


        }

        template<typename vertex_t, typename index_t> requires HasPositionAndNormal<vertex_t>&& std::integral<index_t>
        void ComputeNormalsWeightedByArea(
            std::vector<vertex_t>& vertices,
            std::vector<index_t> indices,
            bool cw = false) 
        {
            std::vector<XMVECTOR> normals(vertices.size());

            for (size_t face = 0; face < indices.size() / 3; ++face)
            {
                index_t i0 = indices[face * 3];
                index_t i1 = indices[face * 3 + 1];
                index_t i2 = indices[face * 3 + 2];

                if (i0 == index_t(-1)
                    || i1 == index_t(-1)
                    || i2 == index_t(-1))
                    continue;

                if (i0 >= vertices.size()
                    || i1 >= vertices.size()
                    || i2 >= vertices.size())
                    return;

                XMVECTOR p0 = XMLoadFloat3(&vertices[i0].position);
                XMVECTOR p1 = XMLoadFloat3(&vertices[i1].position);
                XMVECTOR p2 = XMLoadFloat3(&vertices[i2].position);

                XMVECTOR u = XMVectorSubtract(p1, p0);
                XMVECTOR v = XMVectorSubtract(p2, p0);

                XMVECTOR faceNormal = XMVector3Normalize(XMVector3Cross(u, v));

                // Corner 0 -> 1 - 0, 2 - 0
                XMVECTOR w0 = XMVector3Cross(u, v);
                w0 = XMVector3Length(w0);

                // Corner 1 -> 2 - 1, 0 - 1
                XMVECTOR c = XMVectorSubtract(p2, p1);
                XMVECTOR d = XMVectorSubtract(p0, p1);
                XMVECTOR w1 = XMVector3Cross(c, d);
                w1 = XMVector3Length(w1);

                // Corner 2 -> 0 - 2, 1 - 2
                XMVECTOR e = XMVectorSubtract(p0, p2);
                XMVECTOR f = XMVectorSubtract(p1, p2);
                XMVECTOR w2 = XMVector3Cross(e, f);
                w2 = XMVector3Length(w2);

                normals[i0] = XMVectorMultiplyAdd(faceNormal, w0, normals[i0]);
                normals[i1] = XMVectorMultiplyAdd(faceNormal, w1, normals[i1]);
                normals[i2] = XMVectorMultiplyAdd(faceNormal, w2, normals[i2]);
            }

            // Store results
            if (cw)
            {
                for (size_t vert = 0; vert < vertices.size(); ++vert)
                {
                    XMVECTOR n = XMVector3Normalize(normals[vert]);
                    n = XMVectorNegate(n);
                    XMStoreFloat3(&vertices[vert].normal, n);
                }
            }
            else
            {
                for (size_t vert = 0; vert < vertices.size(); ++vert)
                {
                    XMVECTOR n = XMVector3Normalize(normals[vert]);
                    XMStoreFloat3(&vertices[vert].normal, n);
                }
            }
        }

        static void ComputeTangentFrame(
            _In_reads_(index_count) uint32 const* indices, uint64 index_count,
            _In_reads_(vertex_count) XMFLOAT3 const* positions,
            _In_reads_(vertex_count) XMFLOAT3 const* normals,
            _In_reads_(vertex_count) XMFLOAT2 const* texcoords,
            uint64 vertex_count,
            _Out_writes_opt_(vertex_count) XMFLOAT4* out_tangents)
        {
			XMFLOAT3* tangents = new XMFLOAT3[vertex_count]{}; //use unique_ptr<T[]>
			XMFLOAT3* bitangents = new XMFLOAT3[vertex_count]{};

			for (uint32 i = 0; i <= index_count - 3; i += 3)
			{
				uint32 i0 = indices[i + 0];
				uint32 i1 = indices[i + 1];
				uint32 i2 = indices[i + 2];

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

			for (uint64 i = 0; i < vertex_count; ++i)
			{
				XMFLOAT3 const& n = normals[i];
				XMFLOAT3 const& t = tangents[i];
                XMFLOAT3 const& b = bitangents[i];

				float tangent_w = (XMVectorGetX(XMVector3Dot(XMVector3Cross(XMLoadFloat3(&n), XMLoadFloat3(&t)), XMLoadFloat3(&b))) < 0.0F) ? -1.0F : 1.0F;
				XMVECTOR _out_tangent = XMVector3Normalize(XMVectorSubtract(XMLoadFloat3(&t), XMVector3Dot(XMLoadFloat3(&n), XMLoadFloat3(&t))));
                XMFLOAT3 tangent;
                XMStoreFloat3(&tangent, _out_tangent);
                *(out_tangents + i) = XMFLOAT4(tangent.x, tangent.y, tangent.z, tangent_w);
			}
			delete[] tangents;
        }
    }

    enum class NormalCalculation
    {
        None,
        EqualWeight,
        AngleWeight,
        AreaWeight
    };

    template<typename vertex_t, typename index_t> requires HasPositionAndNormal<vertex_t> && std::integral<index_t>
    void ComputeNormals(
        NormalCalculation normal_type,
        std::vector<vertex_t>& vertices,
        std::vector<index_t> indices,
        bool cw = false)
    {
        switch (normal_type)
        {
        case NormalCalculation::EqualWeight:
            return impl::ComputeNormalsEqualWeight(vertices, indices, cw);
        case NormalCalculation::AngleWeight:
            return impl::ComputeNormalsWeightedByAngle(vertices, indices, cw);
        case NormalCalculation::AreaWeight:
            return impl::ComputeNormalsWeightedByArea(vertices, indices, cw);
        case NormalCalculation::None:
        default:
            return;
        }
    }
   
	inline void ComputeTangentFrame(
		_In_reads_(index_count) uint32 const* indices, uint64 index_count,
		_In_reads_(vertex_count) DirectX::XMFLOAT3 const* positions,
		_In_reads_(vertex_count) DirectX::XMFLOAT3 const* normals,
		_In_reads_(vertex_count) DirectX::XMFLOAT2 const* texcoords,
        uint64 vertex_count,
		_Out_writes_opt_(vertex_count) DirectX::XMFLOAT4* out_tangents)
	{
        impl::ComputeTangentFrame(indices, index_count, positions, normals, texcoords, vertex_count, out_tangents);
	}
}

