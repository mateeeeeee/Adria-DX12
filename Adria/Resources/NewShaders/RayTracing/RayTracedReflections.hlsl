#include "../Constants.hlsli"
#include "../Scene.hlsli"
#include "../Lighting.hlsli"
#include "RayTracingUtil.hlsli"


struct RayTracedReflectionsConstants
{
    float roughnessScale;
    uint  depthIdx;
	uint  outputIdx;
};
ConstantBuffer<RayTracedReflectionsConstants> PassCB : register(b1);

struct RTR_Payload
{
	float3 reflectionColor;
	float  reflectivity;
    uint   randSeed;
};

[shader("raygeneration")]
void RTR_RayGen()
{
	RaytracingAccelerationStructure scene = ResourceDescriptorHeap[FrameCB.accelStructIdx];
	Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];

	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;

	float depth = depthTx.Load(int3(launchIndex.xy, 0));
	float2 texCoords = (launchIndex + 0.5f) / FrameCB.screenResolution;
	float3 posWorld = GetWorldPosition(texCoords, depth);

	RayDesc ray;
	ray.Origin = FrameCB.cameraPosition.xyz;
	ray.Direction = normalize(posWorld - ray.Origin);
	ray.TMin = 0.005f;
	ray.TMax = FLT_MAX;

    uint randSeed = InitRand(launchIndex.x + launchIndex.y * launchDim.x, 0, 16);

	RTR_Payload payloadData;
	payloadData.reflectivity = 0.0f;
	payloadData.reflectionColor = 0.0f;
    payloadData.randSeed = randSeed;
	TraceRay(scene,
		RAY_FLAG_FORCE_OPAQUE,
		0xFF, 0, 0, 0, ray, payloadData);

	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	outputTx[launchIndex.xy] = float4(payloadData.reflectivity * payloadData.reflectionColor, 1.0f);
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

	Texture2D txMetallicRoughness = ResourceDescriptorHeap[materialData.roughnessMetallicIdx];
	float2 roughnessMetallic = txMetallicRoughness.SampleLevel(LinearWrapSampler, uv, 0).gb;

	if (roughnessMetallic.y <= 0.01f) return;

    uint randSeed = payloadData.randSeed;
    float3 dir = GetConeSample(randSeed, reflect(WorldRayDirection(), worldNormal), roughnessMetallic.x * PassCB.roughnessScale);

	RayDesc reflectionRay;
	reflectionRay.Origin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    reflectionRay.Direction = dir;
	reflectionRay.TMin = 0.01f;
	reflectionRay.TMax = FLT_MAX;

	RaytracingAccelerationStructure scene = ResourceDescriptorHeap[FrameCB.accelStructIdx];
	TraceRay(scene,
		RAY_FLAG_FORCE_OPAQUE,
		0xFF, 1, 0, 0, reflectionRay, payloadData);

    payloadData.reflectivity = roughnessMetallic.y * 0.5f;
}

[shader("closesthit")]
void RTR_ClosestHitReflectionRay(inout RTR_Payload payload_data, in HitAttributes attribs)
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
	float2 uv = Interpolate(uv0, uv1, uv2, attribs.barycentrics);

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
	//light.direction.xyz = mul(light.direction.xyz, (float3x3) FrameCB.inverseView);

	float3 radiance = DirectionalLightPBR(light, worldPosition, worldNormal, V, albedoColor.xyz, roughnessMetallic.y, roughnessMetallic.x);

	RayDesc shadowRay;
	shadowRay.Origin = worldPosition;
	shadowRay.Direction = normalize(-light.direction.xyz);
	shadowRay.TMin = 0.2f;
	shadowRay.TMax = FLT_MAX;
	bool visible = TraceShadowRay(shadowRay);

	float3 reflectionColor = visible * radiance + emissiveTx.SampleLevel(LinearWrapSampler, uv, 0).rgb;
	payload_data.reflectionColor = reflectionColor;
}
