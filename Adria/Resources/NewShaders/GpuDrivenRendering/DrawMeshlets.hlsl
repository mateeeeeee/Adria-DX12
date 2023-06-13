#include "GpuDrivenRendering.hlsli"
#include "../Scene.hlsli"

#define BLOCK_SIZE 32

struct DrawMeshletsConstants
{
	uint visibleMeshletsIdx;
};
ConstantBuffer<DrawMeshletsConstants> PassCB : register(b1);


struct VertexOutput
{
	float4 Position     : SV_POSITION;
	float2 Uvs          : TEX;
	float3 NormalVS     : NORMAL0;
	float3 TangentWS    : TANGENT;
	float3 BitangentWS  : BITANGENT;
	float3 NormalWS     : NORMAL1;

	uint   MeshletIndex : TEX1;
};

VertexOutput GetVertex(Mesh mesh, Instance instance, uint vertexId)
{
	VertexOutput vert;
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
	out vertices VertexOutput verts[MESHLET_MAX_VERTICES],
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
		verts[i].MeshletIndex = meshletIndex;
	}

	for (uint j = groupThreadID; j < meshlet.triangleCount; j += BLOCK_SIZE)
	{
		MeshletTriangle tri = LoadMeshBuffer<MeshletTriangle>(mesh.bufferIdx, mesh.meshletTrianglesOffset, meshlet.triangleOffset + j);
		triangles[j] = uint3(tri.V0, tri.V1, tri.V2);
	}
}


struct PixelOutput
{
	float4 NormalMetallic	: SV_TARGET0;
	float4 DiffuseRoughness : SV_TARGET1;
	float4 Emissive			: SV_TARGET2;
};

struct PixelInput
{
	float4 Position		: SV_POSITION;
	float2 Uvs			: TEX;
	float3 NormalVS		: NORMAL0;
	float3 TangentWS	: TANGENT;
	float3 BitangentWS	: BITANGENT;
	float3 NormalWS		: NORMAL1;
	uint   MeshletIndex : TEX1;
	bool   IsFrontFace	: SV_IsFrontFace;
};

PixelOutput PackGBuffer(float3 BaseColor, float3 NormalVS, float4 emissive, float roughness, float metallic)
{
	PixelOutput Out = (PixelOutput)0;
	Out.NormalMetallic = float4(0.5 * NormalVS + 0.5, metallic);
	Out.DiffuseRoughness = float4(BaseColor, roughness);
	Out.Emissive = float4(emissive.rgb, emissive.a / 256);
	return Out;
}

PixelOutput DrawMeshletsPS(PixelInput In)
{
	StructuredBuffer<MeshletCandidate> visibleMeshlets = ResourceDescriptorHeap[PassCB.visibleMeshletsIdx];
	MeshletCandidate candidate = visibleMeshlets[In.MeshletIndex];
	Instance instance = GetInstanceData(candidate.instanceID);
	Material material = GetMaterialData(instance.materialIdx);

	Texture2D txAlbedo = ResourceDescriptorHeap[material.diffuseIdx];
	Texture2D txNormal = ResourceDescriptorHeap[material.normalIdx];
	Texture2D txMetallicRoughness = ResourceDescriptorHeap[material.roughnessMetallicIdx];
	Texture2D txEmissive = ResourceDescriptorHeap[material.emissiveIdx];

	float4 albedoColor = txAlbedo.Sample(LinearWrapSampler, In.Uvs) * float4(material.baseColorFactor, 1.0f);
#if MASK
	if (albedoColor.a < material.alphaCutoff) discard;
#endif

	float3 normal = normalize(In.NormalWS);
	if (In.IsFrontFace) normal = -normal;

	float3 tangent = normalize(In.TangentWS);
	float3 bitangent = normalize(In.BitangentWS);
	float3 bumpMapNormal = txNormal.Sample(LinearWrapSampler, In.Uvs).xyz;
	bumpMapNormal = 2.0f * bumpMapNormal - 1.0f;
	float3x3 TBN = float3x3(tangent, bitangent, normal);
	float3 NewNormal = mul(bumpMapNormal, TBN);
	In.NormalVS = normalize(mul(NewNormal, (float3x3) FrameCB.view));

	float3 aoRoughnessMetallic = txMetallicRoughness.Sample(LinearWrapSampler, In.Uvs).rgb;

	float3 emissiveColor = txEmissive.Sample(LinearWrapSampler, In.Uvs).rgb;
	return PackGBuffer(albedoColor.xyz, normalize(In.NormalVS), float4(emissiveColor, material.emissiveFactor),
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