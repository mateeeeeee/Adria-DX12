#include "../Constants.hlsli"
#include "../CommonResources.hlsli"
#include "RayTracingUtil.hlsli"


struct RayTracedReflectionsConstants
{
    float roughnessScale;
    uint  depthIdx;
	uint  outputIdx;
	uint  verticesIdx;
	uint  indicesIdx;
	uint  geoInfosIdx;
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
	float2 texToords = (launchIndex + 0.5f) / FrameCB.screenResolution;
	float3 posWorld = GetWorldPosition(texToords, depth);

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
	uint geoId = GeometryIndex();
	uint triangleId = PrimitiveIndex();

	StructuredBuffer<Vertex> vertices	= ResourceDescriptorHeap[PassCB.verticesIdx];
	StructuredBuffer<uint> indices		= ResourceDescriptorHeap[PassCB.indicesIdx];
	StructuredBuffer<GeoInfo> geoInfos	= ResourceDescriptorHeap[PassCB.geoInfosIdx];

	GeoInfo geoInfo = geoInfos[geoId];
	uint vbOffset = geoInfo.vertexOffset;
	uint ibOffset = geoInfo.indexOffset;

	uint i0 = indices[ibOffset + triangleId * 3 + 0];
	uint i1 = indices[ibOffset + triangleId * 3 + 1];
	uint i2 = indices[ibOffset + triangleId * 3 + 2];

	Vertex v0 = vertices[vbOffset + i0];
	Vertex v1 = vertices[vbOffset + i1];
	Vertex v2 = vertices[vbOffset + i2];

	float3 pos = Interpolate(v0.pos, v1.pos, v2.pos, attribs.barycentrics);
	float2 uv = Interpolate(v0.uv, v1.uv, v2.uv, attribs.barycentrics); uv.y = 1.0f - uv.y;
	float3 nor = normalize(Interpolate(v0.nor, v1.nor, v2.nor, attribs.barycentrics));
	float3 worldPosition = mul(pos, ObjectToWorld3x4()).xyz;
	float3 worldNormal = mul(nor, (float3x3) WorldToObject4x3());

	Texture2D txMetallicRoughness = ResourceDescriptorHeap[geoInfo.metallicRoughnessIdx];
	float2 roughnessMetallic = txMetallicRoughness.SampleLevel(LinearWrapSampler, uv, 0).gb;

	if (roughnessMetallic.y <= 0.01f) return;
	
    uint randSeed = payloadData.randSeed;
    float3 dir = GetConeSample(randSeed, reflect(WorldRayDirection(), worldNormal), roughnessMetallic.x * PassCB.roughnessScale);

	RayDesc reflectionRay;
	reflectionRay.Origin = WorldRayOrigin() + RayTCurrent() * WorldRayDirection(); //OffsetRay(worldPosition, worldNormal);
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
	uint geoId = GeometryIndex();
	uint triangleId = PrimitiveIndex();

	StructuredBuffer<Vertex> vertices = ResourceDescriptorHeap[PassCB.verticesIdx];
	StructuredBuffer<uint> indices = ResourceDescriptorHeap[PassCB.indicesIdx];
	StructuredBuffer<GeoInfo> geoInfos = ResourceDescriptorHeap[PassCB.geoInfosIdx];

	GeoInfo geoInfo = geoInfos[geoId];
	uint vbOffset = geoInfo.vertexOffset;
	uint ibOffset = geoInfo.indexOffset;

	uint i0 = indices[ibOffset + triangleId * 3 + 0];
	uint i1 = indices[ibOffset + triangleId * 3 + 1];
	uint i2 = indices[ibOffset + triangleId * 3 + 2];

	Vertex v0 = vertices[vbOffset + i0];
	Vertex v1 = vertices[vbOffset + i1];
	Vertex v2 = vertices[vbOffset + i2];

	float2 uv = Interpolate(v0.uv, v1.uv, v2.uv, attribs.barycentrics); uv.y = 1.0f - uv.y;
	Texture2D txAlbedo = ResourceDescriptorHeap[geoInfo.albedoIdx];
    float3 albedo = 0.5f * txAlbedo.SampleLevel(LinearWrapSampler, uv, 2).rgb;
	payload_data.reflectionColor = albedo;
}
