#include "CommonResources.hlsli"

#define MAX_STEPS 16
#define BINARY_SEARCH_STEPS 16

float4 SSRBinarySearch(Texture2D<float> depthTexture, float3 viewDirection, float rayHitThreshold, inout float3 viewHitCoords)
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

    return float4(projectedCoords.xy, depth, abs(depthDiff) < rayHitThreshold ? 1.0f : 0.0f);
}

float4 SSRRayMarch(Texture2D<float> depthTexture, float3 viewDirection, float rayStep, float rayHitThreshold, inout float3 viewHitCoords)
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
            return SSRBinarySearch(depthTexture, viewDirection, rayHitThreshold, viewHitCoords);
        }
        viewDirection *= rayStep;
    }
    return 0.0f;
}