#include "CommonResources.hlsli"

#define BLOCK_SIZE 16

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint  GroupIndex : SV_GroupIndex;
};

struct LightInjectionConstants
{
	uint pad;
};
ConstantBuffer<LightInjectionConstants> PassCB : register(b1);


[numthreads(8, 8, 8)]
void LightInjectionCS(CSInput input)
{
	
}

struct ScatteringAccumulationConstants
{
	uint pad;
};
ConstantBuffer<ScatteringAccumulationConstants> PassCB2 : register(b1);

[numthreads(8, 8, 8)]
void ScatteringAccumulationCS(CSInput input)
{
	
}




