#include "Lighting.hlsli"
#include "Packing.hlsli"
#include "CommonResources.hlsli"

#define BLOCK_SIZE 16

struct RendererDebugViewConstants
{
	uint normalMetallicIdx;
	uint diffuseIdx;
	uint depthIdx;
	uint emissiveIdx;
	uint customIdx;
	uint aoIdx;
	uint motionVectorsIdx;
	uint outputIdx;
	float triangleOverdrawScale;
};
ConstantBuffer<RendererDebugViewConstants> RendererDebugViewPassCB : register(b1);


float3 TurboColormap(float x)
{
	static const float4 RED_COEFFS_4   = float4(0.13572138, 4.61539260, -42.66032258, 132.13108234);
	static const float4 GREEN_COEFFS_4 = float4(0.09140261, 2.19418839, 4.84296658, -14.18503333);
	static const float4 BLUE_COEFFS_4  = float4(0.10667330, 12.64194608, -60.58204836, 110.36276771);
	static const float2 RED_COEFFS_2   = float2(-152.94239396, 59.28637943);
	static const float2 GREEN_COEFFS_2 = float2(4.27729857, 2.82956604);
	static const float2 BLUE_COEFFS_2  = float2(-89.90310912, 27.34824973);

    x = clamp(x, 0.0f, 1.0f); 
    //polynomial basis
    float4 basis_4 = float4(1.0f, x, x * x, x * x * x);
    float2 basis_2 = basis_4.zw * basis_4.z;

    float3 overdrawColor = float3(
        dot(basis_4, RED_COEFFS_4)   + dot(basis_2, RED_COEFFS_2),
        dot(basis_4, GREEN_COEFFS_4) + dot(basis_2, GREEN_COEFFS_2),
        dot(basis_4, BLUE_COEFFS_4)  + dot(basis_2, BLUE_COEFFS_2)
    );
    return overdrawColor;
}

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void RendererDebugViewCS(CSInput input)
{
	Texture2D               normalRT		= ResourceDescriptorHeap[RendererDebugViewPassCB.normalMetallicIdx];
	Texture2D               diffuseRT		= ResourceDescriptorHeap[RendererDebugViewPassCB.diffuseIdx];
	Texture2D<float>        depthTexture	= ResourceDescriptorHeap[RendererDebugViewPassCB.depthIdx];
	RWTexture2D<float4> outputTexture		= ResourceDescriptorHeap[RendererDebugViewPassCB.outputIdx];

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.displayResolution);
	
#if OUTPUT_DIFFUSE || OUTPUT_MIPMAPS  || OUTPUT_MATERIAL_ID || OUTPUT_MESHLET_ID
	float4 albedoRoughness	= diffuseRT.Sample(LinearWrapSampler, uv);
	float3 albedo = albedoRoughness.rgb;
	outputTexture[input.DispatchThreadId.xy] = float4(albedo, 1.0f);

#elif OUTPUT_NORMALS
	float3 viewNormal = DecodeNormalOctahedron(normalRT.Sample(LinearWrapSampler, uv).xy * 2.0f - 1.0f);
	float3 worldNormal = mul(viewNormal, (float3x3)transpose(FrameCB.view));
	worldNormal = normalize(0.5f * worldNormal + 0.5f);
	outputTexture[input.DispatchThreadId.xy] = float4(worldNormal, 1.0f);

#elif OUTPUT_DEPTH 
	float  depth = depthTexture.Sample(LinearWrapSampler, uv);
	float linearDepth = LinearizeDepth(depth);
	float normalizedLinearDepth = linearDepth / FrameCB.cameraNear;
	outputTexture[input.DispatchThreadId.xy] = float4(normalizedLinearDepth, normalizedLinearDepth, normalizedLinearDepth, 1.0f);

#elif OUTPUT_ROUGHNESS
	float4 albedoRoughness	= diffuseRT.Sample(LinearWrapSampler, uv);
	float roughness = albedoRoughness.a;
	outputTexture[input.DispatchThreadId.xy] = float4(roughness, roughness, roughness, 1.0f);

#elif OUTPUT_METALLIC
	float metallic = normalRT.Sample(LinearWrapSampler, uv).z;
	outputTexture[input.DispatchThreadId.xy] = float4(metallic, metallic, metallic, 1.0f);

#elif OUTPUT_EMISSIVE
	Texture2D emissiveTexture = ResourceDescriptorHeap[RendererDebugViewPassCB.emissiveIdx];
	float4 emissiveData = emissiveTexture.Sample(LinearWrapSampler, uv);
	float3 emissiveColor = emissiveData.rgb * emissiveData.a * 256;
	outputTexture[input.DispatchThreadId.xy] = float4(emissiveColor, 1.0f);

#elif OUTPUT_AO
	Texture2D<float> ambientOcclusionTexture = ResourceDescriptorHeap[RendererDebugViewPassCB.aoIdx];
	float ambientOcclusion = ambientOcclusionTexture.Sample(LinearWrapSampler, uv);
	outputTexture[input.DispatchThreadId.xy] = float4(ambientOcclusion, ambientOcclusion, ambientOcclusion, 1.0f);

#elif OUTPUT_INDIRECT
	float4 albedoRoughness	= diffuseRT.Sample(LinearWrapSampler, uv);
	float3 albedo		    = albedoRoughness.rgb;
	float roughness		    = albedoRoughness.a;
	float4 normalMetallic   = normalRT.Sample(LinearWrapSampler, uv);
	float3 viewNormal	    = 2.0f * normalMetallic.rgb - 1.0f;
	float metallic		    = normalMetallic.a;
	BrdfData brdfData	    = GetBrdfData(albedo, metallic, roughness);
	float  depth		    = depthTexture.Sample(LinearWrapSampler, uv);
	float3 viewPosition		= GetViewPosition(uv, depth);

	Texture2D<float> ambientOcclusionTexture = ResourceDescriptorHeap[RendererDebugViewPassCB.aoIdx];
	float ambientOcclusion = ambientOcclusionTexture.Sample(LinearWrapSampler, uv);
	outputTexture[input.DispatchThreadId.xy] = float4(ambientOcclusion, ambientOcclusion, ambientOcclusion, 1.0f);
	float3 indirectLighting = GetIndirectLighting(viewPosition, viewNormal, brdfData.Diffuse, ambientOcclusion);
	outputTexture[input.DispatchThreadId.xy] = float4(indirectLighting * M_PI / brdfData.Diffuse, 1.0f); 

#elif OUTPUT_CUSTOM
	Texture2D customTexture = ResourceDescriptorHeap[RendererDebugViewPassCB.customIdx];
	float4    customData	= customTexture.Sample(LinearWrapSampler, uv);
	outputTexture[input.DispatchThreadId.xy] = customData;

#elif OUTPUT_SHADING_EXTENSION
	//#todo
	uint shadingExtension = uint(normalRT.Sample(LinearWrapSampler, uv).w * 255.0f);
	static float3 Colors[] = 
    {
        float3(1.0, 0.0, 0.0),  
        float3(0.0, 1.0, 0.0),  
        float3(0.0, 0.0, 1.0),  
        float3(1.0, 1.0, 0.0),  
        float3(1.0, 0.0, 1.0),  
        float3(0.0, 1.0, 1.0),  
        float3(0.5, 0.0, 0.5),  
        float3(0.5, 0.5, 0.5)   
    };
	outputTexture[input.DispatchThreadId.xy] = float4(Colors[shadingExtension], 1.0f);
#elif OUTPUT_OVERDRAW
	RWTexture2D<uint> triangleOverdrawTexture = ResourceDescriptorHeap[FrameCB.triangleOverdrawIdx];
	uint2 texCoords = uint2(uv * FrameCB.renderResolution);
	uint overdrawCount = triangleOverdrawTexture.Load(texCoords);
	float overdrawRatio = overdrawCount / (10 * RendererDebugViewPassCB.triangleOverdrawScale);
    outputTexture[input.DispatchThreadId.xy] = float4(TurboColormap(overdrawRatio), 1.0f);
#elif OUTPUT_MOTION_VECTORS
	Texture2D motionVectorsTexture = ResourceDescriptorHeap[RendererDebugViewPassCB.motionVectorsIdx];
	float4    motionVectors	= motionVectorsTexture.Sample(LinearWrapSampler, uv);
	outputTexture[input.DispatchThreadId.xy] = float4(motionVectors.xy * 100, 0.0f, 1.0f);
#else 
	outputTexture[input.DispatchThreadId.xy] = float4(1.0f, 0.0f, 0.0f, 1.0f); 
#endif
}