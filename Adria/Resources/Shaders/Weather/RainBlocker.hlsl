#include "Scene.hlsli"

struct ModelConstants
{
	uint  instanceId;
};
ConstantBuffer<ModelConstants> ModelCB : register(b1);

struct RainBlockerConstants
{
	row_major matrix rainViewProjectionMatrix;
};
ConstantBuffer<RainBlockerConstants> RainBlockerPassCB : register(b2);

struct VSToPS
{
	float4 Pos : SV_POSITION;
};

VSToPS RainBlockerVS(uint VertexID : SV_VertexID)
{
	VSToPS output = (VSToPS)0;
	Instance instanceData = GetInstanceData(ModelCB.instanceId);
	Mesh meshData = GetMeshData(instanceData.meshIndex);
	float3 pos = LoadMeshBuffer<float3>(meshData.bufferIdx, meshData.positionsOffset, VertexID);
	float4 posWS = mul(float4(pos, 1.0f), instanceData.worldMatrix);
	float4 posLS = mul(posWS, RainBlockerPassCB.rainViewProjectionMatrix);
	output.Pos = posLS;
	return output;
}