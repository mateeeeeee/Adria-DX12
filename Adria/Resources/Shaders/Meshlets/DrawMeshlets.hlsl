#include "GpuDrivenRendering.hlsli"
#include "Scene.hlsli"
#include "Packing.hlsli"
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
	uint   CandidateMeshletIndex : TEX2;
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
		Verts[i].CandidateMeshletIndex = candidate.meshletIndex;
	}

	for (uint j = GroupThreadId; j < meshlet.triangleCount; j += BLOCK_SIZE)
	{
		MeshletTriangle tri = LoadMeshBuffer<MeshletTriangle>(mesh.bufferIdx, mesh.meshletTrianglesOffset, meshlet.triangleOffset + j);
		Triangles[j] = uint3(tri.V0, tri.V1, tri.V2);
	}
}

struct PSOutput
{
	float4 NormalRT			: SV_TARGET0;
	float4 DiffuseRT		: SV_TARGET1;
	float4 EmissiveRT		: SV_TARGET2;
	float4 CustomRT			: SV_TARGET3;
};

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
	PSOutput output = (PSOutput)0;

#if VIEW_MIPMAPS
	static const float3 mipColors[6] = 
	{
		float3(1.0f, 0.0f, 0.0f),   // Mip 0: Red
		float3(0.0f, 1.0f, 0.0f),   // Mip 1: Green
		float3(0.0f, 0.0f, 1.0f),   // Mip 2: Blue
		float3(1.0f, 1.0f, 0.0f),   // Mip 3: Yellow
		float3(1.0f, 0.5f, 0.0f),   // Mip 4: Orange
		float3(0.5f, 0.0f, 0.5f)    // Mip 5 and above: Purple
	};
	float mipLevel = albedoTexture.CalculateLevelOfDetail(LinearWrapSampler, input.Uvs);
	mipLevel = clamp(mipLevel, 0.0f, 5.0f);
	int mipColorIndex = round(mipLevel);
	output.DiffuseRT = float4(mipColors[mipColorIndex], 1.0f);
	return output;
#endif
#if MATERIAL_ID
	const uint materialId = instance.materialIdx;
	output.DiffuseRT = float4(UintToColor(materialId), 1.0f);
	return output;
#endif
#if MESHLET_ID
	const uint meshletId = input.CandidateMeshletIndex;
	float3 meshletIdColor = UintToColor(meshletId);
	output.DiffuseRT = float4(meshletIdColor, 1.0f);
	return output;
#endif
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

	float3 viewNormal = normalize(mul(normal, (float3x3) FrameCB.view));
	float4 emissive = float4(emissiveTexture.Sample(LinearWrapSampler, input.Uvs).rgb, material.emissiveFactor);
	float roughness = aoRoughnessMetallic.g * material.roughnessFactor;
	float metallic = aoRoughnessMetallic.b * material.metallicFactor;
	float4 customData = 0.0f;
	uint shadingExtension = ShadingExtension_Default;

#if SHADING_EXTENSION_ANISOTROPY
	//#todo
#elif SHADING_EXTENSION_CLEARCOAT
	float clearCoat = material.clearCoat;
	float clearCoatRoughness = material.clearCoatRoughness;
	
	Texture2D clearCoatTexture = ResourceDescriptorHeap[material.clearCoatIdx];
	Texture2D clearCoatRoughnessTexture = ResourceDescriptorHeap[material.clearCoatRoughnessIdx];
	clearCoat *= clearCoatTexture.Sample(LinearWrapSampler, input.Uvs).r;
	clearCoatRoughness *= clearCoatRoughnessTexture.Sample(LinearWrapSampler, input.Uvs).g;

	Texture2D clearCoatNormalTexture = ResourceDescriptorHeap[material.clearCoatNormalIdx];
    float3 clearCoatNormalTS = normalize(clearCoatNormalTexture.Sample(LinearWrapSampler, input.Uvs).xyz * 2.0f - 1.0f);
    float3 clearCoatNormal = normalize(mul(clearCoatNormal, TBN));
	clearCoatNormalVS = normalize(mul(clearCoatNormal, (float3x3) FrameCB.view));
	customData = EncodeClearCoat(clearCoat, clearCoatRoughness, clearCoatNormalVS);
#endif
	output.NormalRT = EncodeGBufferNormalRT(viewNormal, metallic, shadingExtension);
	output.DiffuseRT = float4(albedoColor.xyz * material.baseColorFactor, roughness);
	output.EmissiveRT = float4(emissive.rgb, emissive.a / 256);
	output.CustomRT = customData;

#if TRIANGLE_OVERDRAW
	RWTexture2D<uint> triangleOverdrawTexture = ResourceDescriptorHeap[FrameCB.triangleOverdrawIdx];
    uint2 texCoords = uint2(input.Position.xy);
	InterlockedAdd(triangleOverdrawTexture[texCoords], 1);
#endif
	return output;
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