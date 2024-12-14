#ifndef _SCENE_
#define _SCENE_
#include "CommonResources.hlsli"

struct Mesh
{
	uint bufferIdx;
	uint positionsOffset;
	uint uvsOffset;
	uint normalsOffset;
	uint tangentsOffset;
	uint indicesOffset;
	uint meshletOffset;
	uint meshletVerticesOffset;
	uint meshletTrianglesOffset;
	uint meshletCount;
};

#define SHADING_EXTENSION_NONE 0
#define SHADING_EXTENSION_CLEARCOAT 1

struct Material
{
    uint    shadingExtension;
	float3	baseColorFactor;
	uint	diffuseIdx;
	uint	roughnessMetallicIdx;
	float	metallicFactor;
	float   roughnessFactor;
	
	uint	normalIdx;
	uint	emissiveIdx;
	float	emissiveFactor;
	float	alphaCutoff;

	uint   clearCoatIdx;
	uint   clearCoatRoughnessIdx;
	uint   clearCoatNormalIdx;
	float  clearCoat;
	float  clearCoatRoughness;
};

struct Instance
{
	row_major matrix worldMatrix;
	row_major matrix inverseWorldMatrix;
	float3 bbOrigin;
	uint pad;
	float3 bbExtents;
	uint instanceId;

	uint materialIdx;
	uint meshIndex;
	uint2 pad2;
};

Instance GetInstanceData(uint instanceId)
{
	StructuredBuffer<Instance> instances = ResourceDescriptorHeap[FrameCB.instancesIdx];
	return instances[instanceId];
}

Mesh GetMeshData(uint meshIdx)
{
	StructuredBuffer<Mesh> meshes = ResourceDescriptorHeap[FrameCB.meshesIdx];
	return meshes[meshIdx];
}

Material GetMaterialData(uint materialIdx)
{
	StructuredBuffer<Material> materials = ResourceDescriptorHeap[FrameCB.materialsIdx];
	return materials[materialIdx];
}

template<typename T>
T LoadMeshBuffer(uint bufferIdx, uint bufferOffset, uint vertexId)
{
	ByteAddressBuffer meshBuffer = ResourceDescriptorHeap[bufferIdx];
	return meshBuffer.Load<T>(bufferOffset + sizeof(T) * vertexId);
}

struct VertexData
{
	float3 pos;
	float2 uv;
	float3 nor;
};

float3 Interpolate(in float3 x0, in float3 x1, in float3 x2, float2 bary)
{
	return x0 * (1.0f - bary.x - bary.y) + bary.x * x1 + bary.y * x2;
}
float2 Interpolate(in float2 x0, in float2 x1, in float2 x2, float2 bary)
{
	return x0 * (1.0f - bary.x - bary.y) + bary.x * x1 + bary.y * x2;
}

VertexData LoadVertexData(Mesh meshData, uint triangleIndex, float2 barycentrics)
{
	uint i0 = LoadMeshBuffer<uint>(meshData.bufferIdx, meshData.indicesOffset, 3 * triangleIndex + 0);
	uint i1 = LoadMeshBuffer<uint>(meshData.bufferIdx, meshData.indicesOffset, 3 * triangleIndex + 1);
	uint i2 = LoadMeshBuffer<uint>(meshData.bufferIdx, meshData.indicesOffset, 3 * triangleIndex + 2);

	float3 pos0 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.positionsOffset, i0);
	float3 pos1 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.positionsOffset, i1);
	float3 pos2 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.positionsOffset, i2);
	float3 pos = Interpolate(pos0, pos1, pos2, barycentrics);

	float2 uv0 = LoadMeshBuffer<float2>(meshData.bufferIdx, meshData.uvsOffset, i0);
	float2 uv1 = LoadMeshBuffer<float2>(meshData.bufferIdx, meshData.uvsOffset, i1);
	float2 uv2 = LoadMeshBuffer<float2>(meshData.bufferIdx, meshData.uvsOffset, i2);
	float2 uv = Interpolate(uv0, uv1, uv2, barycentrics);

	float3 nor0 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.normalsOffset, i0);
	float3 nor1 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.normalsOffset, i1);
	float3 nor2 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.normalsOffset, i2);
	float3 nor = normalize(Interpolate(nor0, nor1, nor2, barycentrics));

	VertexData vertex = (VertexData)0;
	vertex.pos = pos;
	vertex.uv  = uv;
	vertex.nor = nor;
	return vertex;
}

#endif