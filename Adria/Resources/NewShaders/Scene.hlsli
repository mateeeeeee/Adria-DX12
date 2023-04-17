#pragma once
#include "CommonResources.hlsli"

struct Mesh
{
	uint bufferIdx;
	uint positionsOffset;
	uint uvsOffset;
	uint normalsOffset;
	uint tangentsOffset;
	uint bitangentsOffset;
	uint indicesOffset;
	uint meshletOffset;
	uint meshletVerticesOffset;
	uint meshletTrianglesOffset;
	uint meshletCount;
};

struct Material
{
	uint diffuseIdx;
	uint normalIdx;
	uint roughnessMetallicIdx;
	uint emissiveIdx;
	float3 baseColorFactor;
	float emissiveFactor;
	float metallicFactor;
	float roughnessFactor;
	float alphaCutoff;
};

struct Instance
{
	row_major matrix worldMatrix;
	row_major matrix inverseWorldMatrix;
	float3 bbOrigin;
	float3 bbExtents;
	uint instanceId;
	uint materialIdx;
	uint meshIndex;
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
