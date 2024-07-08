#include "CommonResources.hlsli"
#include "Random.hlsli"
#include "Reflections.hlsli"

#define USE_GGX 1
#define BLOCK_SIZE 16
#define MAX_STEPS 16
#define BINARY_SEARCH_STEPS 16

bool NeedReflection(float roughness, float roughness_cutoff)
{
	return roughness < roughness_cutoff;
}

struct SSRConstants
{
    float ssrRayStep;
    float ssrRayHitThreshold;
    uint depthIdx;
    uint normalIdx;
    uint diffuseIdx;
    uint sceneIdx;
    uint outputIdx;
};
ConstantBuffer<SSRConstants> SSRPassCB : register(b1);

float4 SSRBinarySearch(Texture2D<float> depthTexture, float3 viewDirection, inout float3 viewHitCoords)
{
    float depth;
    [unroll(BINARY_SEARCH_STEPS)]
    for (int i = 0; i < BINARY_SEARCH_STEPS; i++)
    {
        float4 projectedCoords = mul(float4(viewHitCoords, 1.0f), FrameCB.projection);
        projectedCoords.xy /= projectedCoords.w;
        projectedCoords.xy = projectedCoords.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

       //linearize depth here
        depth = depthTexture.SampleLevel(PointClampSampler, projectedCoords.xy, 0);
        float3 fPositionVS = GetViewPosition(projectedCoords.xy, depth);
        float fDepthDiff = viewHitCoords.z - fPositionVS.z;

        if (fDepthDiff <= 0.0f)
            viewHitCoords += viewDirection;

        viewDirection *= 0.5f;
        viewHitCoords -= viewDirection;
    }

    float4 projectedCoords = mul(float4(viewHitCoords, 1.0f), FrameCB.projection);
    projectedCoords.xy /= projectedCoords.w;
    projectedCoords.xy = projectedCoords.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

    depth = depthTexture.SampleLevel(PointClampSampler, projectedCoords.xy, 0);
    float3 viewPosition = GetViewPosition(projectedCoords.xy, depth);
    float depthDiff = viewHitCoords.z - viewPosition.z;

    return float4(projectedCoords.xy, depth, abs(depthDiff) < SSRPassCB.ssrRayHitThreshold ? 1.0f : 0.0f);
}
float4 SSRRayMarch(Texture2D<float> depthTexture, float3 viewDirection, inout float3 viewHitCoords)
{
    float depth;
    for (int i = 0; i < MAX_STEPS; i++)
    {
        viewHitCoords += viewDirection;
        float4 projectedCoords = mul(float4(viewHitCoords, 1.0f), FrameCB.projection);
        projectedCoords.xy /= projectedCoords.w;
        projectedCoords.xy = projectedCoords.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
		depth = depthTexture.SampleLevel(PointClampSampler, projectedCoords.xy, 0);
        float3 viewPosition = GetViewPosition(projectedCoords.xy, depth);
        float depthDiff = viewHitCoords.z - viewPosition.z;
		[branch]
        if (depthDiff > 0.0f)
        {
            return SSRBinarySearch(depthTexture, viewDirection, viewHitCoords);
        }
        viewDirection *= SSRPassCB.ssrRayStep;
    }
    return 0.0f;
}
bool InsideScreen(in float2 coords)
{
    return !(coords.x < 0 || coords.x > 1 || coords.y < 0 || coords.y > 1);
}

struct CSInput
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void SSR_CS(CSInput input)
{
    Texture2D normalTexture = ResourceDescriptorHeap[SSRPassCB.normalIdx];
    Texture2D diffuseTexture = ResourceDescriptorHeap[SSRPassCB.diffuseIdx];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[SSRPassCB.depthIdx];
    Texture2D sceneTexture = ResourceDescriptorHeap[SSRPassCB.sceneIdx];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[SSRPassCB.outputIdx];

    float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);
    float4 viewNormalMetallic = normalTexture.Sample(LinearBorderSampler, uv);
    float3 viewNormal = viewNormalMetallic.rgb;
    viewNormal = 2.0f * viewNormal - 1.0f;
    viewNormal = normalize(viewNormal);

    float roughness = diffuseTexture.Sample(LinearBorderSampler, uv).a;
	float4 sceneColor = sceneTexture.SampleLevel(LinearClampSampler, uv, 0);

    if (roughness >= ROUGHNESS_CUTOFF)
    {
        outputTexture[input.DispatchThreadId.xy] = sceneColor;
        return;
    }

    uint randSeed = InitRand(input.DispatchThreadId.x + input.DispatchThreadId.y * BLOCK_SIZE, 0, 16);
    float2 rand = float2(NextRand(randSeed), NextRand(randSeed));

    float depth = depthTexture.Sample(LinearClampSampler, uv);
    float3 viewPosition = GetViewPosition(uv, depth);
#if USE_GGX
    float3 reflectDir = normalize(ReflectionDir_GGX(viewPosition, viewNormal, roughness, rand).xyz);
#else 
    float3 reflectDir = normalize(reflect(viewPosition, viewNormal));
#endif

    float3 HitPos = viewPosition;
    float4 coords = SSRRayMarch(depthTexture, reflectDir, viewPosition);
    float2 coordsEdgeFactors = float2(1, 1) - pow(saturate(abs(coords.xy - float2(0.5f, 0.5f)) * 2), 8);
    float  screenEdgeFactor = saturate(min(coordsEdgeFactors.x, coordsEdgeFactors.y));
    float4 fresnel = clamp(pow(1 - dot(normalize(viewPosition), viewNormal), 1), 0, 1);

    float reflectionIntensity =
		saturate(
			screenEdgeFactor *
			saturate(reflectDir.z)
			* (coords.w)
			);
    float4 reflectionColor = fresnel * reflectionIntensity * float4(sceneTexture.SampleLevel(LinearClampSampler, coords.xy, 0).rgb, 1.0f);
    outputTexture[input.DispatchThreadId.xy] = sceneColor + max(0, reflectionColor);
}