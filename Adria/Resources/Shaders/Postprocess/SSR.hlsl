#include "SSRCommon.hlsli"
#include "Random.hlsli"
#include "BRDF.hlsli"
#include "Packing.hlsli"

#define USE_GGX 0
#define BLOCK_SIZE 16

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
    Texture2D normalRT = ResourceDescriptorHeap[SSRPassCB.normalIdx];
    Texture2D diffuseRT = ResourceDescriptorHeap[SSRPassCB.diffuseIdx];
    Texture2D<float> depthTexture = ResourceDescriptorHeap[SSRPassCB.depthIdx];
    Texture2D sceneTexture = ResourceDescriptorHeap[SSRPassCB.sceneIdx];
    RWTexture2D<float4> outputTexture = ResourceDescriptorHeap[SSRPassCB.outputIdx];

    float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);
    float depth = depthTexture.SampleLevel(LinearClampSampler, uv, 0);
	float4 sceneColor = sceneTexture.SampleLevel(LinearClampSampler, uv, 0);
    if (depth <= 0.00001f)
    {
        outputTexture[input.DispatchThreadId.xy] = sceneColor;
        return;
    }

    float3 viewNormal = DecodeNormalOctahedron(normalRT.Sample(LinearBorderSampler, uv).xy * 2.0f - 1.0f);
    float roughness = diffuseRT.Sample(LinearBorderSampler, uv).a;

    if (roughness >= ROUGHNESS_CUTOFF)
    {
        outputTexture[input.DispatchThreadId.xy] = sceneColor;
        return;
    }
    
    float3 viewPosition = GetViewPosition(uv, depth);
#if USE_GGX
    RNG rng = RNG_Initialize(input.DispatchThreadId.x + input.DispatchThreadId.y * BLOCK_SIZE, 0, 16);
    float2 rand = float2(RNG_GetNext(rng), RNG_GetNext(rng));
    float3 reflectDir = normalize(ReflectionDir_GGX(viewPosition, viewNormal, roughness, rand).xyz);
#else 
    float3 reflectDir = normalize(reflect(viewPosition, viewNormal));
#endif

    float4 coords = SSRRayMarch(depthTexture, reflectDir, SSRPassCB.ssrRayStep, SSRPassCB.ssrRayHitThreshold, viewPosition);
    float2 coordsEdgeFactors = float2(1, 1) - pow(saturate(abs(coords.xy - float2(0.5f, 0.5f)) * 2), 8);
    float  screenEdgeFactor = saturate(min(coordsEdgeFactors.x, coordsEdgeFactors.y));

    float3 hitColor = sceneTexture.SampleLevel(LinearClampSampler, coords.xy, 0).rgb;
    float roughnessMask = saturate(1.0f - (roughness / ROUGHNESS_CUTOFF));
    roughnessMask *= roughnessMask;
    float4 fresnel = clamp(pow(1 - dot(normalize(viewPosition), viewNormal), 1), 0, 1);

    float4 reflectionColor = float4(saturate(hitColor.xyz * screenEdgeFactor * roughnessMask), 1.0f);
    outputTexture[input.DispatchThreadId.xy] = sceneColor + fresnel * max(0, reflectionColor);
}