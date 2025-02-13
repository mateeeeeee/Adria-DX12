#ifndef  _SURFACE_
#define  _SURFACE_
#include "BRDF.hlsli"
#include "CommonResources.hlsli"

struct Surface
{
    float3   worldPos;
    float3   viewDir;
    float3   worldNormal;
    //float3 geoNormal; 
    float    depth;
    float    viewDepth;
    float    diffuseProbability;
    BrdfData brdfData;
};

Surface GetEmptySurface()
{
    Surface emptySurface = (Surface)0;
    return emptySurface;
}

float CalculateLuminance(float3 color)
{
	return dot(color, float3(0.2126729, 0.7151522, 0.0721750));
}

float GetSurfaceDiffuseProbability(Surface surface)
{
    BrdfData brdfData = surface.brdfData;
    float diffuseWeight = CalculateLuminance(brdfData.Diffuse);
    float specularWeight = CalculateLuminance(F_Schlick(dot(surface.viewDir, surface.normal), brdfData.Specular));
    float sumWeights = diffuseWeight + specularWeight;
    return sumWeights < 1e-7f ? 1.f : (diffuseWeight / sumWeights);
}

Surface GetSurface(uint2 pixelCoords, uint albedoIndex, uint normalIndex, uint depthIndex)
{
    Surface surface = GetEmptySurface();
    Texture2D<float> depthTexture = ResourceDescriptorHeap[depthIdx];
    Texture2D<float4> normalRT = ResourceDescriptorHeap[normalIdx];
    Texture2D<float4> albedoRT = ResourceDescriptorHeap[albedoIdx];

    if(depthRT[pixelCoords].r == 0)
    {
        return surface;
    }

    surface.depth = depthRT[pixelCoords].r;
    surface.viewDepth = LinearizeDepth(surface.depth);

    float3 viewNormal; float metallic; uint  shadingExtension;
	float4 normalRTData = normalRT[pixelCoords];
	DecodeGBufferNormalRT(normalRTData, viewNormal, metallic, shadingExtension);

    viewNormal = 2.0f * viewNormal - 1.0f;
    surface.worldNormal = normalize(mul(viewNormal, (float3x3)FrameCB.view));
    surface.worldPos = GetWorldPosition(FullScreenPosition(pixelCoords), depth);
    surface.viewDir = normalize(FrameCB.cameraPosition - worldPos);

    float4 albedoRoughness = albedoRT.Sample(LinearWrapSampler, uv);
	float3 albedo = albedoRoughness.rgb; float  roughness = albedoRoughness.a;
    surface.brdfData = GetBrdfData(albedo, metallic, roughness);
    surface.diffuseProbability = GetSurfaceDiffuseProbability(surface);
    return surface;
}


























#endif