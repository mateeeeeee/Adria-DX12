#include "GpuDrivenRendering.hlsli"
#include "Scene.hlsli"

#define BLOCK_SIZE 32

struct DrawMeshletsConstants
{
	uint visibleMeshletsIdx;
};
ConstantBuffer<DrawMeshletsConstants> PassCB : register(b1);


struct MSToPS
{
	float4 Position     : SV_POSITION;
	float2 Uvs          : TEX;
	float3 TangentWS    : TANGENT;
	float3 BitangentWS  : BITANGENT;
	float3 NormalWS     : NORMAL1;
	uint   MeshletIndex : TEX1;
};

MSToPS GetVertex(Mesh mesh, Instance instance, uint vertexId)
{
	MSToPS output;
	float3 pos = LoadMeshBuffer<float3>(mesh.bufferIdx,  mesh.positionsOffset, vertexId);
	float2 uv  = LoadMeshBuffer<float2>(mesh.bufferIdx,  mesh.uvsOffset, vertexId);
	float3 nor = LoadMeshBuffer<float3>(mesh.bufferIdx,  mesh.normalsOffset, vertexId);
	float4 tan = LoadMeshBuffer<float4>(mesh.bufferIdx,  mesh.tangentsOffset, vertexId);
	
	float4 posWS = mul(float4(pos, 1.0), instance.worldMatrix);
	output.Position = mul(posWS, FrameCB.viewProjection);
	output.Position.xy += FrameCB.cameraJitter * output.Position.w;
	output.Uvs = uv;

	output.NormalWS =  mul(nor, (float3x3) transpose(instance.inverseWorldMatrix));;
	output.TangentWS = mul(tan.xyz, (float3x3) instance.worldMatrix);
	output.BitangentWS = normalize(cross(output.NormalWS, output.TangentWS) * tan.w);
	return output;
}


[outputtopology("triangle")]
[numthreads(BLOCK_SIZE, 1, 1)]
void DrawMeshletsMS(
	in uint groupThreadId : SV_GroupIndex,
	in uint groupId : SV_GroupID,
	out vertices MSToPS verts[MESHLET_MAX_VERTICES],
	out indices uint3 triangles[MESHLET_MAX_TRIANGLES])
{
	StructuredBuffer<MeshletCandidate> visibleMeshlets = ResourceDescriptorHeap[PassCB.visibleMeshletsIdx];
	uint meshletIndex = groupId;

	MeshletCandidate candidate = visibleMeshlets[meshletIndex];
	Instance instance = GetInstanceData(candidate.instanceID);
	Mesh mesh = GetMeshData(instance.meshIndex);
	Meshlet meshlet = GetMeshletData(mesh.bufferIdx, mesh.meshletOffset, candidate.meshletIndex);
	SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);

	for (uint i = groupThreadId; i < meshlet.vertexCount; i += BLOCK_SIZE)
	{
		uint vertexId = LoadMeshBuffer<uint>(mesh.bufferIdx, mesh.meshletVerticesOffset, meshlet.vertexOffset + i);
		verts[i] = GetVertex(mesh, instance, vertexId);
		verts[i].MeshletIndex = meshletIndex;
	}

	for (uint j = groupThreadId; j < meshlet.triangleCount; j += BLOCK_SIZE)
	{
		MeshletTriangle tri = LoadMeshBuffer<MeshletTriangle>(mesh.bufferIdx, mesh.meshletTrianglesOffset, meshlet.triangleOffset + j);
		triangles[j] = uint3(tri.V0, tri.V1, tri.V2);
	}
}

struct PSOutput
{
	float4 NormalMetallic	: SV_TARGET0;
	float4 DiffuseRoughness : SV_TARGET1;
	float4 Emissive			: SV_TARGET2;
};

PSOutput PackGBuffer(float3 BaseColor, float3 NormalVS, float4 emissive, float roughness, float metallic)
{
	PSOutput output = (PSOutput)0;
	output.NormalMetallic = float4(0.5 * NormalVS + 0.5, metallic);
	output.DiffuseRoughness = float4(BaseColor, roughness);
	output.Emissive = float4(emissive.rgb, emissive.a / 256);
	return output;
}

PSOutput DrawMeshletsPS(MSToPS input)
{
	StructuredBuffer<MeshletCandidate> visibleMeshlets = ResourceDescriptorHeap[PassCB.visibleMeshletsIdx];
	MeshletCandidate candidate = visibleMeshlets[input.MeshletIndex];
	Instance instance = GetInstanceData(candidate.instanceID);
	Material material = GetMaterialData(instance.materialIdx);

	Texture2D albedoTx = ResourceDescriptorHeap[material.diffuseIdx];
	Texture2D normalTx = ResourceDescriptorHeap[material.normalIdx];
	Texture2D metallicRoughnessTx = ResourceDescriptorHeap[material.roughnessMetallicIdx];
	Texture2D emissiveTx = ResourceDescriptorHeap[material.emissiveIdx];

	float4 albedoColor = albedoTx.Sample(LinearWrapSampler, input.Uvs) * float4(material.baseColorFactor, 1.0f);
	if (albedoColor.a < material.alphaCutoff) discard;

	float3 normal = normalize(input.NormalWS);
	float3 tangent = normalize(input.TangentWS);
	float3 bitangent = normalize(input.BitangentWS);
	float3x3 TBN = float3x3(tangent, bitangent, normal); 
	float3 normalTS = normalTx.Sample(LinearWrapSampler, input.Uvs).xyz;
	normalTS.xy = 2.0f * normalTS.xy - 1.0f;
	normalTS.z = sqrt(1.0f - normalTS.x * normalTS.x - normalTS.y * normalTS.y);
	normal = mul(normalTS, TBN);
#if RAIN
#endif
	float3 normalVS = normalize(mul(normal, (float3x3) FrameCB.view));

	float3 aoRoughnessMetallic = metallicRoughnessTx.Sample(LinearWrapSampler, input.Uvs).rgb;
	float3 emissiveColor = emissiveTx.Sample(LinearWrapSampler, input.Uvs).rgb;
	return PackGBuffer(albedoColor.xyz, normalVS, float4(emissiveColor, material.emissiveFactor),
		aoRoughnessMetallic.g * material.roughnessFactor, aoRoughnessMetallic.b * material.metallicFactor);
}


struct BuildMeshletDrawArgsConstants
{
	uint visibleMeshletsCounterIdx;
	uint meshletDrawArgsIdx;
};
ConstantBuffer<BuildMeshletDrawArgsConstants> PassCB2 : register(b1);


[numthreads(1, 1, 1)]
void BuildMeshletDrawArgsCS()
{
	RWBuffer<uint> visibleMeshletsCounter = ResourceDescriptorHeap[PassCB2.visibleMeshletsCounterIdx];
	RWStructuredBuffer<uint3> meshletDrawArgs = ResourceDescriptorHeap[PassCB2.meshletDrawArgsIdx];

#if !SECOND_PHASE
	uint numMeshlets = visibleMeshletsCounter[COUNTER_PHASE1_VISIBLE_MESHLETS];
#else
	uint numMeshlets = visibleMeshletsCounter[COUNTER_PHASE2_VISIBLE_MESHLETS];
#endif
	uint3 args = uint3(ceil(numMeshlets / 1.0f), 1, 1);
	meshletDrawArgs[0] = args;
}