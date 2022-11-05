#include "../CommonResources.hlsli"
#include "RayTracingUtil.hlsli"

struct RayTracedAmbientOcclusionConstants
{
	uint  depthIdx;
	uint  gbufNormals;
	uint  outputIdx;
	float aoRadius;
};

ConstantBuffer<RayTracedAmbientOcclusionConstants> PassCB : register(b1);


struct AORayData
{
	bool hit;
};

static const int RAY_COUNT = 16;

[shader("raygeneration")]
void RTAO_RayGen()
{
    RaytracingAccelerationStructure scene = ResourceDescriptorHeap[FrameCB.accelStructIdx];
	Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
	Texture2D gbufferNormals = ResourceDescriptorHeap[PassCB.gbufNormals];
	RWTexture2D<float> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];

	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDim = DispatchRaysDimensions().xy;

	float depth = depthTx.Load(int3(launchIndex.xy, 0)).r;
	float2 texCoords = (launchIndex + 0.5f) / FrameCB.screenResolution;
	float3 worldPosition = GetWorldPosition(texCoords, depth);

	float3 viewNormal = gbufferNormals.Load(int3(launchIndex.xy, 0)).xyz;
	viewNormal = 2 * viewNormal - 1.0;
	float3 worldNormal = normalize(mul(viewNormal, (float3x3) transpose(FrameCB.view)));
	uint randSeed = InitRand(launchIndex.x + launchIndex.y * launchDim.x, FrameCB.frameCount, 16);

	float ao = 0.0f;
	[unroll(RAY_COUNT)]
	for (int i = 0; i < RAY_COUNT; i++)
	{
		float3 worldDir = GetCosHemisphereSample(randSeed, worldNormal);
		AORayData rayPayload = { true };
		RayDesc rayAO;
		rayAO.Origin = OffsetRay(worldPosition, worldNormal);
		rayAO.Direction = normalize(worldDir);
		rayAO.TMin = 0.05f;
		rayAO.TMax = PassCB.aoRadius;

		TraceRay(scene,
			(RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES),
			0xFF, 0, 0, 0, rayAO, rayPayload);
		ao += rayPayload.hit ? 0.0f : 1.0f;
	}

	outputTx[launchIndex.xy] = ao / RAY_COUNT;
}

[shader("miss")]
void RTAO_Miss(inout AORayData hitData)
{
	hitData.hit = false;
}

[shader("anyhit")]
void RTAO_AnyHit(inout AORayData hitData, in BuiltInTriangleIntersectionAttributes attribs)
{
	hitData.hit = true;
}