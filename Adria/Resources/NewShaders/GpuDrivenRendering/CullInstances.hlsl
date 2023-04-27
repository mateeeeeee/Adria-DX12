#include "../CommonResources.hlsli"
#include "../Common.hlsli"

#define BLOCK_SIZE 64

[numthreads(BLOCK_SIZE, 1, 1)]
void CullInstancesCS(uint3 DTid : SV_DispatchThreadID)
{

}