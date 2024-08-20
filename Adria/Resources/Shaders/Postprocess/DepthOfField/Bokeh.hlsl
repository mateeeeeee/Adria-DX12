#include "CommonResources.hlsli"

#ifndef KARIS_INVERSE
#define KARIS_INVERSE 0
#endif

#define BLOCK_SIZE 16

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void BokehFirstPassCS(CSInput input)
{
}


[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void BokehSecondPassCS(CSInput input)
{
}