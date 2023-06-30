#include "DDGICommon.hlsli"
#include "../Lighting.hlsli"
#include "../Common.hlsli"
#include "../RayTracing/RayTracingUtil.hlsli"

struct DDGIRayTracePassConstants
{
	float3 randomVector;
	float  randomAngle;
	float  historyBlendWeight;
	uint   rayBufferIdx;
};
ConstantBuffer<DDGIRayTracePassConstants> PassCB : register(b1);
ConstantBuffer<DDGIVolume> DDGIVolumeCB			 : register(b2);

struct DDGIPayload
{
	float3 radiance;
	float  distance;
};


[shader("raygeneration")]
void DDGI_RayGen()
{
	RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[FrameCB.accelStructIdx];

	uint probeIdx = DispatchRaysIndex().y;
	uint rayIdx = DispatchRaysIndex().x;

	float3 probeLocation = GetProbeLocation(DDGIVolumeCB, probeIdx);
	float3x3 randomRotation = AngleAxis3x3(PassCB.randomAngle, PassCB.randomVector);
	float3  randomDirection = normalize(mul(SphericalFibonacci(rayIdx, DDGIVolumeCB.raysPerProbe), randomRotation));

	RayDesc ray;
	ray.Origin = probeLocation;
	ray.Direction = randomDirection;
	ray.TMin = 0.01f;
	ray.TMax = FLT_MAX;

	DDGIPayload payload;
	payload.radiance = float3(0, 0, 0);
	payload.distance = FLT_MAX;
	TraceRay(tlas, (RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES), 0xFF, 0, 0, 0, ray, payload);
	RWBuffer<float4> rayBuffer = ResourceDescriptorHeap[PassCB.rayBufferIdx];

	rayBuffer[probeIdx * DDGIVolumeCB.maxRaysPerProbe + rayIdx] = float4(payload.radiance, payload.distance);
}

[shader("miss")]
void DDGI_Miss(inout DDGIPayload payload)
{
	TextureCube envMap = ResourceDescriptorHeap[FrameCB.envMapIdx];
	payload.radiance = envMap.SampleLevel(LinearWrapSampler, WorldRayDirection(), 0).rgb;
	payload.distance = FLT_MAX;
}

[shader("closesthit")]
void DDGI_ClosestHit(inout DDGIPayload payload, in HitAttributes attribs)
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
	float2 uv = Interpolate(uv0, uv1, uv2, attribs.barycentrics);

	float3 nor0 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.normalsOffset, i0);
	float3 nor1 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.normalsOffset, i1);
	float3 nor2 = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.normalsOffset, i2);
	float3 nor = normalize(Interpolate(nor0, nor1, nor2, attribs.barycentrics));

	float4 posWS = mul(float4(pos, 1.0), instanceData.worldMatrix);
	float3 worldPosition = posWS.xyz / posWS.w;
	float3 worldNormal = mul(nor, (float3x3) transpose(instanceData.inverseWorldMatrix));

	MaterialProperties matProperties = GetMaterialProperties(materialData, uv, 0);
	BrdfData brdfData = GetBrdfData(matProperties);

	float3 V = normalize(FrameCB.cameraPosition.xyz - worldPosition.xyz);
	float3 N = worldNormal;

	//#todo: use all lights
	StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];
	Light light = lights[0];
	light.direction.xyz = mul(light.direction.xyz, (float3x3) FrameCB.inverseView);

	float3 L = normalize(-light.direction.xyz); //#todo: make a function TraceShadowRay(light,...)
	RayDesc shadowRay;
	shadowRay.Origin = worldPosition.xyz;
	shadowRay.Direction = L;
	shadowRay.TMin = 1e-2f;
	shadowRay.TMax = FLT_MAX;
	bool visibility = TraceShadowRay(shadowRay) ? 1.0f : 0.0f;
	if (!visibility) return;

	float3 diffuse = saturate(dot(L, N)) * Diffuse_Lambert(brdfData.Diffuse);
	float3 radiance = diffuse * light.color.rgb;

	radiance += matProperties.emissive;
	radiance += Diffuse_Lambert(min(brdfData.Diffuse, 0.9f)) * SampleDDGIIrradiance(DDGIVolumeCB, worldPosition, N, WorldRayDirection());
	
	payload.radiance = radiance;
	payload.distance = RayTCurrent();
}