#include "CommonResources.hlsli"
#include "Constants.hlsli"

#define BLOCK_SIZE 16
#define LAMBDA 1.2

struct OceanNormalsConstants
{
	float fftResolution;
	float oceanSize;
	float oceanChoppiness;
	uint  displacementIdx;
	uint  outputIdx;
};
ConstantBuffer<OceanNormalsConstants> OceanNormalsPassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void OceanNormalsCS(CSInput input)
{
	Texture2D<float4> displacementTexture = ResourceDescriptorHeap[OceanNormalsPassCB.displacementIdx];
	RWTexture2D<float4> normalTexture = ResourceDescriptorHeap[OceanNormalsPassCB.outputIdx];

	uint2 pixelCoord = input.DispatchThreadId.xy;
	float texel = 1.f / OceanNormalsPassCB.fftResolution;
	float texelSize = OceanNormalsPassCB.oceanSize * texel;

	float3 displacementRight = displacementTexture.Load(uint3(uint2(clamp(pixelCoord.x + 1, 0, OceanNormalsPassCB.fftResolution - 1), pixelCoord.y), 0)).xyz;
	float3 displacementLeft = displacementTexture.Load(uint3(uint2(clamp(pixelCoord.x - 1, 0, OceanNormalsPassCB.fftResolution - 1), pixelCoord.y), 0)).xyz;
	float3 displacementTop = displacementTexture.Load(uint3(uint2(pixelCoord.x, clamp(pixelCoord.y - 1, 0, OceanNormalsPassCB.fftResolution - 1)), 0)).xyz;
	float3 displacementBottom = displacementTexture.Load(uint3(uint2(pixelCoord.x, clamp(pixelCoord.y + 1, 0, OceanNormalsPassCB.fftResolution - 1)), 0)).xyz;

	float3 center = displacementTexture.Load(uint3(pixelCoord, 0)).xyz;
	float3 right = float3(texelSize, 0.f, 0.f) + displacementRight;
	float3 left = float3(-texelSize, 0.f, 0.f) + displacementLeft;
	float3 top = float3(0.f, 0.f, -texelSize) + displacementTop;
	float3 bottom = float3(0.f, 0.f, texelSize) + displacementBottom;

	float3 topRight = cross(right - center, top - center);
	float3 topLeft = cross(top - center, left - center);
	float3 bottomLeft = cross(left - center, bottom - center);
	float3 bottomRight = cross(bottom - center, right - center);

	float2 dDx = (displacementRight.xz - displacementLeft.xz) * 0.5f;
	float2 dDy = (displacementBottom.xz - displacementTop.xz) * 0.5f;

	float J = (1.0 + dDx.x * LAMBDA) * (1.0 + dDy.y * LAMBDA) - dDx.y * dDy.x * LAMBDA * LAMBDA;
	float foamFactor = max(-J, 0.0); //negative J means foam
	normalTexture[pixelCoord] = float4(normalize(topRight + bottomRight + topLeft + bottomLeft), foamFactor);
}