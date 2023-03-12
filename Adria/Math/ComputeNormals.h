#pragma once
#include <DirectXMath.h>
#include <vector>
#include <concepts>
#include "../Core/Definitions.h"


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
    }

    enum class NormalCalculation
    {
        None,
        EqualWeight,
        AngleWeight,
        AreaWeight
    };

    template<typename vertex_t, typename index_t> requires HasPositionAndNormal<vertex_t>&& std::integral<index_t>
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
   
}

