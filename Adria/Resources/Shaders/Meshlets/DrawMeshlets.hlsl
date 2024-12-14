#include "GpuDrivenRendering.hlsli"
#include "Scene.hlsli"
#if RAIN
#include "Weather/RainUtil.hlsli"
#endif

#define BLOCK_SIZE 32

struct DrawMeshletsConstants
{
	uint visibleMeshletsIdx;
};
ConstantBuffer<DrawMeshletsConstants> DrawMeshletsPassCB : register(b1);


struct MSToPS
{
	float4 Position     : SV_POSITION;
	float2 Uvs          : TEX;
	float3 PositionWS	: POSITION;
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
	output.PositionWS = posWS.xyz;
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
	in uint GroupThreadId : SV_GroupIndex,
	in uint GroupId : SV_GroupID,
	out vertices MSToPS Verts[MESHLET_MAX_VERTICES],
	out indices uint3 Triangles[MESHLET_MAX_TRIANGLES])
{
	StructuredBuffer<MeshletCandidate> visibleMeshletsBuffer = ResourceDescriptorHeap[DrawMeshletsPassCB.visibleMeshletsIdx];
	uint meshletIndex = GroupId;

	MeshletCandidate candidate = visibleMeshletsBuffer[meshletIndex];
	Instance instance = GetInstanceData(candidate.instanceID);
	Mesh mesh = GetMeshData(instance.meshIndex);
	Meshlet meshlet = GetMeshletData(mesh.bufferIdx, mesh.meshletOffset, candidate.meshletIndex);
	SetMeshOutputCounts(meshlet.vertexCount, meshlet.triangleCount);

	for (uint i = GroupThreadId; i < meshlet.vertexCount; i += BLOCK_SIZE)
	{
		uint vertexId = LoadMeshBuffer<uint>(mesh.bufferIdx, mesh.meshletVerticesOffset, meshlet.vertexOffset + i);
		Verts[i] = GetVertex(mesh, instance, vertexId);
		Verts[i].MeshletIndex = meshletIndex;
	}

	for (uint j = GroupThreadId; j < meshlet.triangleCount; j += BLOCK_SIZE)
	{
		MeshletTriangle tri = LoadMeshBuffer<MeshletTriangle>(mesh.bufferIdx, mesh.meshletTrianglesOffset, meshlet.triangleOffset + j);
		Triangles[j] = uint3(tri.V0, tri.V1, tri.V2);
	}
}

struct PSOutput
{
	float4 NormalMetallic	: SV_TARGET0;
	float4 DiffuseRoughness : SV_TARGET1;
	float4 Emissive			: SV_TARGET2;
	float4 Custom			: SV_TARGET3;
};

PSOutput PackGBuffer(float3 BaseColor, float3 NormalVS, float4 emissive, float roughness, float metallic, uint extension, float3 customData)
{
	PSOutput output = (PSOutput)0;
	output.NormalMetallic = float4(0.5f * NormalVS + 0.5f, metallic);
	output.DiffuseRoughness = float4(BaseColor, roughness);
	output.Emissive = float4(emissive.rgb, emissive.a / 256);
	output.Custom = float4((float)extension / 255.0f, customData);
	return output;
}

PSOutput DrawMeshletsPS(MSToPS input)
{
	StructuredBuffer<MeshletCandidate> visibleMeshletsBuffer = ResourceDescriptorHeap[DrawMeshletsPassCB.visibleMeshletsIdx];
	MeshletCandidate candidate = visibleMeshletsBuffer[input.MeshletIndex];
	Instance instance = GetInstanceData(candidate.instanceID);
	Material material = GetMaterialData(instance.materialIdx);

	Texture2D albedoTexture = ResourceDescriptorHeap[material.diffuseIdx];
	Texture2D normalTexture = ResourceDescriptorHeap[material.normalIdx];
	Texture2D metallicRoughnessTexture = ResourceDescriptorHeap[material.roughnessMetallicIdx];
	Texture2D emissiveTexture = ResourceDescriptorHeap[material.emissiveIdx];

	float4 albedoColor = albedoTexture.Sample(LinearWrapSampler, input.Uvs) * float4(material.baseColorFactor, 1.0f);
	if (albedoColor.a < material.alphaCutoff) discard;

	float3 normal = normalize(input.NormalWS);
	float3 tangent = normalize(input.TangentWS);
	float3 bitangent = normalize(input.BitangentWS);
    float3 normalTS = normalize(normalTexture.Sample(LinearWrapSampler, input.Uvs).xyz * 2.0f - 1.0f);
    float3x3 TBN = float3x3(tangent, bitangent, normal); 
    normal = normalize(mul(normalTS, TBN));

	float3 aoRoughnessMetallic = metallicRoughnessTexture.Sample(LinearWrapSampler, input.Uvs).rgb;
#if RAIN
	ApplyRain(input.PositionWS.xyz, albedoColor.rgb, aoRoughnessMetallic.g, normal, tangent, bitangent);
#endif
	float3 normalVS = normalize(mul(normal, (float3x3) FrameCB.view));

	float3 emissiveColor = emissiveTexture.Sample(LinearWrapSampler, input.Uvs).rgb;
	return PackGBuffer(albedoColor.xyz * material.baseColorFactor, normalVS, float4(emissiveColor, material.emissiveFactor),
		aoRoughnessMetallic.g * material.roughnessFactor, aoRoughnessMetallic.b * material.metallicFactor, 0, 0.0f);
}


struct BuildMeshletDrawArgsConstants
{
	uint visibleMeshletsCounterIdx;
	uint meshletDrawArgsIdx;
};
ConstantBuffer<BuildMeshletDrawArgsConstants> BuildMeshletDrawArgsPassCB : register(b1);


[numthreads(1, 1, 1)]
void BuildMeshletDrawArgsCS()
{
	RWBuffer<uint> visibleMeshletsCounter = ResourceDescriptorHeap[BuildMeshletDrawArgsPassCB.visibleMeshletsCounterIdx];
	RWStructuredBuffer<uint3> meshletDrawArgsBuffer = ResourceDescriptorHeap[BuildMeshletDrawArgsPassCB.meshletDrawArgsIdx];

#if !SECOND_PHASE
	uint numMeshlets = visibleMeshletsCounter[COUNTER_PHASE1_VISIBLE_MESHLETS];
#else
	uint numMeshlets = visibleMeshletsCounter[COUNTER_PHASE2_VISIBLE_MESHLETS];
#endif
	uint3 args = uint3(ceil(numMeshlets / 1.0f), 1, 1);
	meshletDrawArgsBuffer[0] = args;
}