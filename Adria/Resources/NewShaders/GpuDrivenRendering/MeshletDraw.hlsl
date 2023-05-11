#include "GpuDrivenRendering.hlsli"
#include "../Scene.hlsli"

#define BLOCK_SIZE 32

struct DrawMeshlets1stPhaseConstants
{
	uint visibleMeshletsIdx;
};
ConstantBuffer<DrawMeshlets1stPhaseConstants> PassCB : register(b1);


struct Vertex
{
	float4 Position     : SV_POSITION;
	float2 Uvs          : TEX;
	float3 NormalVS     : NORMAL0;
	float3 TangentWS    : TANGENT;
	float3 BitangentWS  : BITANGENT;
	float3 NormalWS     : NORMAL1;
};

Vertex GetVertex(Mesh mesh, Instance instance, uint vertexId)
{
	Vertex vert;
	float3 pos = LoadMeshBuffer<float3>(mesh.bufferIdx,  mesh.positionsOffset, vertexId);
	float2 uv  = LoadMeshBuffer<float2>(mesh.bufferIdx,  mesh.uvsOffset, vertexId);
	float3 nor = LoadMeshBuffer<float3>(mesh.bufferIdx,  mesh.normalsOffset, vertexId);
	float4 tan = LoadMeshBuffer<float4>(mesh.bufferIdx,  mesh.tangentsOffset, vertexId);
	float3 bit = LoadMeshBuffer<float3>(mesh.bufferIdx,  mesh.bitangentsOffset, vertexId);

	float4 posWS = mul(float4(pos, 1.0), instance.worldMatrix);
	vert.Position = mul(posWS, FrameCB.viewProjection);
	vert.Position.xy += FrameCB.cameraJitter * vert.Position.w;

	vert.Uvs = uv;

	float3 normalWS = mul(nor, (float3x3) transpose(instance.inverseWorldMatrix));
	vert.NormalVS = mul(normalWS, (float3x3) transpose(FrameCB.inverseView));
	vert.TangentWS = mul(tan.xyz, (float3x3) instance.worldMatrix);
	vert.BitangentWS = mul(bit, (float3x3) instance.worldMatrix);
	vert.NormalWS = normalWS;

	return vert;
}


[outputtopology("triangle")]
[numthreads(BLOCK_SIZE, 1, 1)]
void DrawMeshletsMS(
	in uint groupThreadID : SV_GroupIndex,
	in uint groupID : SV_GroupID,
	out vertices Vertex verts[MESHLET_MAX_VERTICES],
	out indices uint3 triangles[MESHLET_MAX_TRIANGLES])
{
	StructuredBuffer<MeshletCandidate> visibleMeshlets = ResourceDescriptorHeap[PassCB.visibleMeshletsIdx];
	uint meshletIndex = groupID;

	MeshletCandidate candidate = visibleMeshlets[meshletIndex];
	Instance instance = GetInstanceData(candidate.instanceID);
	Mesh mesh = GetMeshData(instance.meshIndex);
	Meshlet meshlet = GetMeshletData(mesh.bufferIdx, mesh.meshletOffset, candidate.meshletIndex);
	SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);

	for (uint i = groupThreadID; i < meshlet.vertexCount; i += BLOCK_SIZE)
	{
		uint vertexId = LoadMeshBuffer<uint>(mesh.bufferIdx, mesh.meshletVerticesOffset, meshlet.vertexOffset + i);
		verts[i] = GetVertex(mesh, instance, vertexId);
	}

	for (uint j = groupThreadID; j < meshlet.triangleCount; j += BLOCK_SIZE)
	{
		MeshletTriangle tri = LoadMeshBuffer<MeshletTriangle>(mesh.bufferIdx, mesh.meshletTrianglesOffset, meshlet.triangleOffset + j);
		triangles[j] = uint3(tri.V0, tri.V1, tri.V2);
	}
}