#include "../CommonResources.hlsli"

#define BLOCK_SIZE 16
#define MAX_STEPS 16
#define BINARY_SEARCH_STEPS 16

struct SSRConstants
{
    float ssrRayStep;
    float ssrRayHitThreshold;
    
    uint depthIdx;
    uint normalIdx;
    uint sceneIdx;
    uint outputIdx;
};
ConstantBuffer<SSRConstants> PassCB : register(b1);

float4 SSRBinarySearch(Texture2D<float> depthTx, float3 viewDirection, inout float3 viewHitCoords)
{
    float depth;
    [unroll(BINARY_SEARCH_STEPS)]
    for (int i = 0; i < BINARY_SEARCH_STEPS; i++)
    {
        float4 projectedCoords = mul(float4(viewHitCoords, 1.0f), FrameCB.projection);
        projectedCoords.xy /= projectedCoords.w;
        projectedCoords.xy = projectedCoords.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

       //linearize depth here
        depth = depthTx.SampleLevel(PointClampSampler, projectedCoords.xy, 0);
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

    depth = depthTx.SampleLevel(PointClampSampler, projectedCoords.xy, 0);
    float3 viewPosition = GetViewPosition(projectedCoords.xy, depth);
    float depthDiff = viewHitCoords.z - viewPosition.z;

    return float4(projectedCoords.xy, depth, abs(depthDiff) < PassCB.ssrRayHitThreshold ? 1.0f : 0.0f);
}
float4 SSRRayMarch(Texture2D<float> depthTx, float3 viewDirection, inout float3 viewHitCoords)
{
    float depth;
    for (int i = 0; i < MAX_STEPS; i++)
    {
        viewHitCoords += viewDirection;
        float4 projectedCoords = mul(float4(viewHitCoords, 1.0f), FrameCB.projection);
        projectedCoords.xy /= projectedCoords.w;
        projectedCoords.xy = projectedCoords.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
		depth = depthTx.SampleLevel(PointClampSampler, projectedCoords.xy, 0);
        float3 viewPosition = GetViewPosition(projectedCoords.xy, depth);
        float depthDiff = viewHitCoords.z - viewPosition.z;
		[branch]
        if (depthDiff > 0.0f)
        {
            return SSRBinarySearch(depthTx, viewDirection, viewHitCoords);
        }
        viewDirection *= PassCB.ssrRayStep;
    }
    return 0.0f;
}
bool InsideScreen(in float2 coords)
{
    return !(coords.x < 0 || coords.x > 1 || coords.y < 0 || coords.y > 1);
}

struct CS_INPUT
{
    uint3 GroupId : SV_GroupID;
    uint3 GroupThreadId : SV_GroupThreadID;
    uint3 DispatchThreadId : SV_DispatchThreadID;
    uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void SSR(CS_INPUT input)
{
    Texture2D normalTx = ResourceDescriptorHeap[PassCB.normalIdx];
    Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
    Texture2D sceneTx = ResourceDescriptorHeap[PassCB.sceneIdx];
    RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
    
    float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.screenResolution);
    float4 viewNormalMetallic = normalTx.Sample(LinearBorderSampler, uv);
    float3 viewNormal = viewNormalMetallic.rgb;
    viewNormal = 2.0f * viewNormal - 1.0f;
    viewNormal = normalize(viewNormal);
    
    float metallic = viewNormalMetallic.a;
    float4 sceneColor = sceneTx.SampleLevel(LinearClampSampler, uv, 0);
    if (metallic < 0.01f)
    {
        outputTx[input.DispatchThreadId.xy] = sceneColor;
        return;
    }

    float depth = depthTx.Sample(LinearClampSampler, uv);
    float3 viewPosition = GetViewPosition(uv, depth);
    float3 reflectDir = normalize(reflect(viewPosition, viewNormal));
	
	//Raycast
    float3 HitPos = viewPosition;
    float4 coords = SSRRayMarch(depthTx, reflectDir, viewPosition);
    float2 coordsEdgeFactors = float2(1, 1) - pow(saturate(abs(coords.xy - float2(0.5f, 0.5f)) * 2), 8);
    float  screenEdgeFactor = saturate(min(coordsEdgeFactors.x, coordsEdgeFactors.y));
	
    float reflectionIntensity =
		saturate(
			screenEdgeFactor * 
			saturate(reflectDir.z) 
			* (coords.w) 
			);
    float4 reflectionColor = reflectionIntensity * float4(sceneTx.SampleLevel(LinearClampSampler, coords.xy, 0).rgb, 1.0f);
    outputTx[input.DispatchThreadId.xy] = sceneColor + metallic * max(0, reflectionColor);
}