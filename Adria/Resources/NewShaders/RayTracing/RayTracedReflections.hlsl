#include "../Constants.hlsli"
#include "../Scene.hlsli"
#include "../Lighting.hlsli"
#include "RayTracingUtil.hlsli"


struct RayTracedReflectionsConstants
{
    float roughnessScale;
    uint  depthIdx;
	uint  normalIdx;
	uint  outputIdx;
};
ConstantBuffer<RayTracedReflectionsConstants> PassCB : register(b1);

struct RTR_Payload
{
	float3 reflectionColor;
	uint   randSeed;
};

[shader("raygeneration")]
void RTR_RayGen()
{
	RaytracingAccelerationStructure scene = ResourceDescriptorHeap[FrameCB.accelStructIdx];
	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
	Texture2D normalMetallicTx = ResourceDescriptorHeap[PassCB.normalIdx];

	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;

	float2 uv = (launchIndex + 0.5f) / FrameCB.screenResolution;
	float depth = depthTx.Load(int3(launchIndex.xy, 0));
	float4 normalMetallic = normalMetallicTx.Load(int3(launchIndex.xy, 0));

	float metallic = normalMetallic.a;
	if (metallic <= 0.01f)
	{
		outputTx[launchIndex.xy] = 0.0f;
		return;
	}

	float3 viewNormal = normalMetallic.xyz;
	viewNormal = 2 * viewNormal - 1.0;
	float3 worldNormal = normalize(mul(viewNormal, (float3x3) transpose(FrameCB.view)));
	float3 worldPosition = GetWorldPosition(uv, depth);

	float3 V = normalize(worldPosition - FrameCB.cameraPosition.xyz);
	float3 rayDir = reflect(V, worldNormal);

	uint randSeed = InitRand(launchIndex.x + launchIndex.y * launchDim.x, 0, 16);
	rayDir = GetConeSample(randSeed, rayDir, PassCB.roughnessScale);

	RayDesc ray;
	ray.Origin = worldPosition;
	ray.Direction = rayDir;
	ray.TMin = 0.02f;
	ray.TMax = FLT_MAX;

	RTR_Payload payloadData;
	payloadData.reflectionColor = 0.0f;
    payloadData.randSeed = randSeed;
	TraceRay(scene,
		RAY_FLAG_FORCE_OPAQUE,
		0xFF, 0, 0, 0, ray, payloadData);

	outputTx[launchIndex.xy] = float4(payloadData.reflectionColor, 1.0f);
}

[shader("miss")]
void RTR_Miss(inout RTR_Payload payloadData)
{
    TextureCube envMap = ResourceDescriptorHeap[FrameCB.envMapIdx];
    payloadData.reflectionColor = envMap.SampleLevel(LinearWrapSampler, WorldRayDirection(), 0).rgb;
}

[shader("closesthit")]
void RTR_ClosestHitPrimaryRay(inout RTR_Payload payloadData, in HitAttributes attribs)
{
	StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];
	uint triangleId = PrimitiveIndex();

	Instance instanceData = GetInstanceData(InstanceIndex());
	Mesh meshData = GetMeshData(instanceData.meshIndex);
	Material materialData = GetMaterialData(instanceData.materialIdx);

	uint i0 = LoadMeshBuffer<uint>(meshData.bufferIdx, meshData.indicesOffset, 3 * triangleId + 0);
	uint i1 = LoadMeshBuffer<uint>(meshData.bufferIdx, meshData.indicesOffset, 3 * triangleId + 1);
	uint i2 = LoadMeshBuffer<uint>(meshData.bufferIdx, meshData.indicesOffset, 3 * triangleId + 2);

	float3 pos0 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.positionsOffset, i0);
	float3 pos1 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.positionsOffset, i1);
	float3 pos2 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.positionsOffset, i2);
	float3 pos = Interpolate(pos0, pos1, pos2, attribs.barycentrics);

	float2 uv0 = LoadMeshBuffer<float2>(meshData.bufferIdx, meshData.uvsOffset, i0);
	float2 uv1 = LoadMeshBuffer<float2>(meshData.bufferIdx, meshData.uvsOffset, i1);
	float2 uv2 = LoadMeshBuffer<float2>(meshData.bufferIdx, meshData.uvsOffset, i2);
	float2 uv  = Interpolate(uv0, uv1, uv2, attribs.barycentrics);

	float3 nor0 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.normalsOffset, i0);
	float3 nor1 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.normalsOffset, i1);
	float3 nor2 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.normalsOffset, i2);
	float3 nor = normalize(Interpolate(nor0, nor1, nor2, attribs.barycentrics));

	float3 worldPosition = pos;
    float3 worldNormal = nor;

	Texture2D albedoTx = ResourceDescriptorHeap[materialData.diffuseIdx];
	Texture2D emissiveTx = ResourceDescriptorHeap[materialData.emissiveIdx];
	Texture2D txMetallicRoughness = ResourceDescriptorHeap[materialData.roughnessMetallicIdx];

	float4 albedoColor = albedoTx.SampleLevel(LinearWrapSampler, uv, 0) * float4(materialData.baseColorFactor, 1.0f);
	float2 roughnessMetallic = txMetallicRoughness.SampleLevel(LinearWrapSampler, uv, 0).gb;

	float3 V = normalize(FrameCB.cameraPosition.xyz - worldPosition);

	Light light = lights[0];
	light.direction.xyz = mul(light.direction.xyz, (float3x3) FrameCB.inverseView);
	float3 radiance = DirectionalLightPBR(light, worldPosition, worldNormal, V, albedoColor.xyz, roughnessMetallic.y, roughnessMetallic.x);

	float3 L = -light.direction.xyz;

	bool visibility = false;
	{
		RaytracingAccelerationStructure raytracingAS = ResourceDescriptorHeap[FrameCB.accelStructIdx];
		RayQuery<RAY_FLAG_CULL_NON_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;

		RayDesc shadowRay;
		shadowRay.Origin = worldPosition;
		shadowRay.Direction = normalize(L);
		shadowRay.TMin = 0.01f;
		shadowRay.TMax = FLT_MAX;

		q.TraceRayInline(raytracingAS, RAY_FLAG_NONE, 0xFF, shadowRay);
		while (q.Proceed())
		{
			switch (q.CandidateType())
			{
			case CANDIDATE_NON_OPAQUE_TRIANGLE:
			{
				q.CommitNonOpaqueTriangleHit();
				break;
			}
			}
		}
		visibility = q.CommittedStatus() != COMMITTED_TRIANGLE_HIT;
	}

	float3 reflectionColor = visibility * radiance + emissiveTx.SampleLevel(LinearWrapSampler, uv, 0).rgb;
	payloadData.reflectionColor = reflectionColor;
}


