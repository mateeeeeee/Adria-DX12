#include "../CommonResources.hlsli"
#include "../Packing.hlsli"

#define BLOCK_SIZE 16

struct LightingConstants
{
	uint  normalMetallicIdx;
	uint  diffuseIdx;
	uint  depthIdx;
	uint  outputIdx;
};
ConstantBuffer<LightingConstants> PassCB : register(b1);

struct CS_INPUT
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void DeferredLighting(CS_INPUT input)
{

}