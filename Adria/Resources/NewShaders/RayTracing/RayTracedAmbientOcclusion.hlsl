#include "RayTracingUtil.hlsli"

struct RayTracedAmbientOcclusionConstants
{
	uint  depthIdx;
	uint  normalsIdx;
	uint  outputIdx;
	float aoRadius;
	float aoPower;
};

ConstantBuffer<RayTracedAmbientOcclusionConstants> PassCB : register(b1);


struct AORayData
{
	float tHit;
};

static const int RAY_COUNT = 1;

[shader("raygeneration")]
void RTAO_RayGen()
{
    RaytracingAccelerationStructure tlas = ResourceDescriptorHeap[FrameCB.accelStructIdx];
	Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
	Texture2D normalsTx = ResourceDescriptorHeap[PassCB.normalsIdx];
	RWTexture2D<float> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];

	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;

	float depth = depthTx.Load(int3(launchIndex.xy, 0)).r;
	float2 texCoords = (launchIndex + 0.5f) / FrameCB.screenResolution;
	float3 worldPosition = GetWorldPosition(texCoords, depth);
	float3 viewNormal = normalsTx.Load(int3(launchIndex.xy, 0)).xyz;
	viewNormal = 2 * viewNormal - 1.0;
	float3 worldNormal = normalize(mul(viewNormal, (float3x3) transpose(FrameCB.view)));

	uint randSeed = InitRand(launchIndex.x + launchIndex.y * launchDim.x, FrameCB.frameCount, 16);

	float3 worldDir = GetCosHemisphereSample(randSeed, worldNormal);
	AORayData rayPayload = { true };
	RayDesc rayAO;
	rayAO.Origin = OffsetRay(worldPosition, worldNormal);
	rayAO.Direction = normalize(worldDir);
	rayAO.TMin = 0.02f;
	rayAO.TMax = PassCB.aoRadius;

	TraceRay(tlas,
		(RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES),
		0xFF, 0, 0, 0, rayAO, rayPayload);

	outputTx[launchIndex.xy] = rayPayload.tHit < 0.0f ? 1.0f : pow(saturate(rayPayload.tHit / PassCB.aoRadius), PassCB.aoPower);
}

[shader("miss")]
void RTAO_Miss(inout AORayData hitData)
{
	hitData.tHit = -1;
}

[shader("anyhit")]
void RTAO_AnyHit(inout AORayData hitData, in BuiltInTriangleIntersectionAttributes attribs)
{
	hitData.tHit = RayTCurrent();
}