#include "../Lighting.hlsli"
#include "../Packing.hlsli"
#include "../DDGI/DDGICommon.hlsli"

#define BLOCK_SIZE 16

struct DeferredLightingConstants
{
	uint normalMetallicIdx;
	uint diffuseIdx;
	uint depthIdx;
	uint outputIdx;
};
ConstantBuffer<DeferredLightingConstants> PassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void DeferredLighting(CSInput input)
{
	Texture2D               normalMetallicTx = ResourceDescriptorHeap[PassCB.normalMetallicIdx];
	Texture2D               diffuseTx		 = ResourceDescriptorHeap[PassCB.diffuseIdx];
	Texture2D<float>        depthTx			 = ResourceDescriptorHeap[PassCB.depthIdx];
	StructuredBuffer<Light> lights		     = ResourceDescriptorHeap[FrameCB.lightsIdx];

	uint lightCount, _unused;
	lights.GetDimensions(lightCount, _unused);

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.screenResolution);

	float4 normalMetallic = normalMetallicTx.Sample(LinearWrapSampler, uv);
	float3 viewNormal	  = 2.0f * normalMetallic.rgb - 1.0f;
	float  metallic		  = normalMetallic.a;
	float  depth		  = depthTx.Sample(LinearWrapSampler, uv);

	float3 viewPosition		= GetViewPosition(uv, depth);
	float4 albedoRoughness	= diffuseTx.Sample(LinearWrapSampler, uv);
	float3 V				= normalize(float3(0.0f, 0.0f, 0.0f) - viewPosition);
	float3 albedo			= albedoRoughness.rgb;
	float  roughness		= albedoRoughness.a;

	BrdfData brdfData = GetBrdfData(albedo, metallic, roughness);
	LightingResult lightResult = (LightingResult)0;
	for (uint i = 0; i < lightCount; ++i)
	{
		Light light = lights[i];
		if (!light.active) continue;
        lightResult = lightResult + DoLight(light, brdfData, viewPosition, viewNormal, V, uv);
    }
	
	//#todo finish this, move viewPosition and viewNormal to World space, remove completely ambient pass and add it here,
	if (FrameCB.ddgiVolumesIdx >= 0)
	{
		StructuredBuffer<DDGIVolume> ddgiVolumes = ResourceDescriptorHeap[FrameCB.ddgiVolumesIdx];
		DDGIVolume ddgiVolume = ddgiVolumes[0];
		//lightResult.Diffuse *= SampleDDGIIrradiance(viewPosition, viewNormal, -V);
	}
	

	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	outputTx[input.DispatchThreadId.xy] += float4(lightResult.Diffuse + lightResult.Specular, 1.0f);
}