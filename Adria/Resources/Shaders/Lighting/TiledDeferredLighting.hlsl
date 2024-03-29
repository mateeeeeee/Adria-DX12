#include "Lighting.hlsli"
#include "Packing.hlsli"

#define BLOCK_SIZE 16
#define MAX_TILE_LIGHTS 256

struct TiledLightingConstants
{
	int  maxLightsVisualization;
	uint normalMetallicIdx;
	uint diffuseIdx;
	uint depthIdx;
	uint emissiveIdx;
	uint aoIdx;
	uint outputIdx;
	int  debugIdx;
};
ConstantBuffer<TiledLightingConstants> PassCB : register(b1);

groupshared uint MinZ;
groupshared uint MaxZ;
groupshared uint TileLightIndices[MAX_TILE_LIGHTS];
groupshared uint TileNumLights;

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void TiledDeferredLighting(CSInput input)
{
	Texture2D               normalMetallicTx = ResourceDescriptorHeap[PassCB.normalMetallicIdx];
	Texture2D               diffuseTx		 = ResourceDescriptorHeap[PassCB.diffuseIdx];
	Texture2D<float>        depthTx			 = ResourceDescriptorHeap[PassCB.depthIdx];

	StructuredBuffer<Light> lights = ResourceDescriptorHeap[FrameCB.lightsIdx];
	uint totalLights, _unused;
	lights.GetDimensions(totalLights, _unused);

	float2 uv = ((float2) input.DispatchThreadId.xy + 0.5f) * 1.0f / (FrameCB.renderResolution);

	float minZ = FrameCB.cameraFar;
	float maxZ = FrameCB.cameraNear;

	float depth = depthTx.Load(int3(input.DispatchThreadId.xy, 0));
	float linearDepth = LinearizeDepth(depth);

	bool validPixel = linearDepth >= FrameCB.cameraNear && linearDepth < FrameCB.cameraFar;

	[flatten]
	if (validPixel)
	{
		minZ = min(minZ, linearDepth);
		maxZ = max(maxZ, linearDepth);
	}

	if (input.GroupIndex == 0)
	{
		TileNumLights = 0;

		MinZ = 0x7F7FFFFF;
		MaxZ = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	if (maxZ >= minZ)
	{
		InterlockedMin(MinZ, asuint(minZ));
		InterlockedMax(MaxZ, asuint(maxZ));
	}

	GroupMemoryBarrierWithGroupSync();

	float tileMinZ = asfloat(minZ);
	float tileMaxZ = asfloat(maxZ);

	float2 tileScale = FrameCB.renderResolution * rcp(float(2 * BLOCK_SIZE));
	float2 tileBias = tileScale - float2(input.GroupId.xy);

	float4 c1 = float4(FrameCB.projection._11 * tileScale.x, 0.0f, tileBias.x, 0.0f);
	float4 c2 = float4(0.0f, -FrameCB.projection._22 * tileScale.y, tileBias.y, 0.0f);
	float4 c4 = float4(0.0f, 0.0f, 1.0f, 0.0f);

	float4 frustumPlanes[6];
	frustumPlanes[0] = c4 - c1;
	frustumPlanes[1] = c4 + c1;
	frustumPlanes[2] = c4 - c2;
	frustumPlanes[3] = c4 + c2;
	frustumPlanes[4] = float4(0.0f, 0.0f, 1.0f, -tileMinZ);
	frustumPlanes[5] = float4(0.0f, 0.0f, -1.0f, tileMaxZ);

	[unroll]
	for (uint i = 0; i < 4; ++i)
	{
		frustumPlanes[i] *= rcp(length(frustumPlanes[i].xyz));
	}

	for (uint lightIndex = input.GroupIndex; lightIndex < totalLights; lightIndex += BLOCK_SIZE * BLOCK_SIZE)
	{
		Light light = lights[lightIndex];
		if (!light.active) continue;
		bool inFrustum = true;
		if (light.type != DIRECTIONAL_LIGHT)
		{
			[unroll]
			for (uint i = 0; i < 6; ++i)
			{
				float d = dot(frustumPlanes[i], float4(light.position.xyz, 1.0f));
				inFrustum = inFrustum && (d >= -light.range);
			}
		}

		[branch]
		if (inFrustum)
		{
			uint listIndex;
			InterlockedAdd(TileNumLights, 1, listIndex);
			TileLightIndices[listIndex] = lightIndex;
		}
	}

	GroupMemoryBarrierWithGroupSync();

	float3 viewPosition = GetViewPosition(uv, depth);
	float4 normalMetallic = normalMetallicTx.Load(int3(input.DispatchThreadId.xy, 0));
	float3 viewNormal  = 2.0f * normalMetallic.rgb - 1.0f;
	float metallic = normalMetallic.a;
	float4 albedoRoughness = diffuseTx.Load(int3(input.DispatchThreadId.xy, 0));
	float3 V = normalize(0.0f.xxx - viewPosition);
	float3 albedo = albedoRoughness.rgb;
	float  roughness = albedoRoughness.a;

	BrdfData brdfData = GetBrdfData(albedo, metallic, roughness);
	LightingResult lightResult = (LightingResult)0; 
	if (all(input.DispatchThreadId.xy < FrameCB.renderResolution))
	{
		for (int i = 0; i < TileNumLights; ++i)
		{
			Light light = lights[TileLightIndices[i]];
			if (!light.active) continue;
            lightResult = lightResult + DoLight(light, brdfData, viewPosition, viewNormal, V, uv);
        }
	}

	Texture2D<float> aoTx = ResourceDescriptorHeap[PassCB.aoIdx];
	float ambientOcclusion = aoTx.Sample(LinearWrapSampler, uv);
	float3 indirectLighting = GetIndirectLighting(viewPosition, viewNormal, brdfData.Diffuse, ambientOcclusion);

	Texture2D emissiveTx = ResourceDescriptorHeap[PassCB.emissiveIdx];
	float4 emissiveData = emissiveTx.Sample(LinearWrapSampler, uv);
	float3 emissiveColor = emissiveData.rgb * emissiveData.a * 256;
	
	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	outputTx[input.DispatchThreadId.xy] = float4(indirectLighting + lightResult.Diffuse + lightResult.Specular + emissiveColor, 1.0f);

	if (PassCB.debugIdx > 0)
	{
		const float3 heatMap[] =
		{
			float3(0, 0, 0),
			float3(0, 0, 1),
			float3(0, 1, 1),
			float3(0, 1, 0),
			float3(1, 1, 0),
			float3(1, 0, 0),
		};
		const uint heatMapMax = 5;
		float  l = saturate((float)TileNumLights / PassCB.maxLightsVisualization) * heatMapMax;
		float3 a = heatMap[floor(l)];
		float3 b = heatMap[ceil(l)];

		float4 debugColor = float4(lerp(a, b, l - floor(l)), 0.5f);

		RWTexture2D<float4> debugTx = ResourceDescriptorHeap[PassCB.debugIdx];
		debugTx[input.DispatchThreadId.xy] = debugColor;
	}
}