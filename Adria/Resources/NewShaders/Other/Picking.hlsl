#include "../CommonResources.hlsli"

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
ConstantBuffer<PickingConstants> PassCB : register(b1);

[numthreads(1, 1, 1)]
void Picking()
{
	if (any(FrameCB.mouseNormalizedCoords > 1.0f) || any(FrameCB.mouseNormalizedCoords < 0.0f)) return;

	Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
	Texture2D<float4> normalTx = ResourceDescriptorHeap[PassCB.normalIdx];
	RWStructuredBuffer<PickingData> pickingBuffer = ResourceDescriptorHeap[PassCB.bufferIdx];

	uint2 mouseCoords = uint2(FrameCB.mouseNormalizedCoords * FrameCB.screenResolution);
	float2 uv = (mouseCoords + 0.5f) / FrameCB.screenResolution;

	float zw = depthTx[mouseCoords].x;
	uv = uv * 2.0f - 1.0f;
	uv.y *= -1.0f;
	float4 worldPosition = mul(float4(uv, zw, 1.0f), FrameCB.inverseViewProjection);
	float3 viewNormal = normalTx[mouseCoords].xyz;
	viewNormal = 2.0f * viewNormal - 1.0f;

	PickingData pickingData;
	pickingData.position = worldPosition / worldPosition.w;
	pickingData.normal = float4(normalize(mul(viewNormal, (float3x3) transpose(FrameCB.view))), 0.0f);
	pickingBuffer[0] = pickingData;
}