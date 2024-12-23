#include "CommonResources.hlsli"
#include "Packing.hlsli"

struct PickingData
{
	float4 position;
	float4 normal;
};

struct PickingConstants
{
	uint depthIdx;
	uint normalIdx;
	uint bufferIdx;
};
ConstantBuffer<PickingConstants> PickingPassCB : register(b1);

[numthreads(1, 1, 1)]
void PickingCS()
{
	if (any(FrameCB.mouseNormalizedCoords > 1.0f) || any(FrameCB.mouseNormalizedCoords < 0.0f)) return;

	Texture2D<float> depthTexture = ResourceDescriptorHeap[PickingPassCB.depthIdx];
	Texture2D<float4> normalRT = ResourceDescriptorHeap[PickingPassCB.normalIdx];
	RWStructuredBuffer<PickingData> pickingBuffer = ResourceDescriptorHeap[PickingPassCB.bufferIdx];

	uint2 mouseCoords = uint2(FrameCB.mouseNormalizedCoords * FrameCB.renderResolution);
	float2 uv = (mouseCoords + 0.5f) / FrameCB.renderResolution;

	float zw = depthTexture[mouseCoords].x;
	uv = uv * 2.0f - 1.0f;
	uv.y *= -1.0f;
	float4 worldPosition = mul(float4(uv, zw, 1.0f), FrameCB.inverseViewProjection);
	float3 viewNormal = DecodeNormalOctahedron(2.0f * normalRT[mouseCoords].xy - 1.0f);

	PickingData pickingData;
	pickingData.position = worldPosition / worldPosition.w;
	pickingData.normal = float4(normalize(mul(viewNormal, (float3x3) transpose(FrameCB.view))), 0.0f);
	pickingBuffer[0] = pickingData;
}