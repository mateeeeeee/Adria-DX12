#include "../Util/RootSignatures.hlsli"

#define SORT_SIZE       512
#define ITEMS_PER_GROUP	( SORT_SIZE )
#define HALF_SIZE		(SORT_SIZE/2)

#define ITERATIONS		(HALF_SIZE > 1024 ? HALF_SIZE/1024 : 1)
#define NUM_THREADS		(HALF_SIZE/ITERATIONS)
#define INVERSION		(16*2 + 8*3)


cbuffer NumElementsCB : register(b11)
{
    int4 NumElements;
};

RWStructuredBuffer<float2> Data : register(u0);

groupshared float2 LDS[SORT_SIZE];

[RootSignature(Sort512_RS)]
[numthreads(NUM_THREADS, 1, 1)]
void main(uint3 Gid : SV_GroupID,
		  uint3 DTid : SV_DispatchThreadID,
		  uint3 GTid : SV_GroupThreadID,
		  uint GI : SV_GroupIndex)
{
    int GlobalBaseIndex = (Gid.x * SORT_SIZE) + GTid.x;
    int LocalBaseIndex = GI;

    int numElementsInThreadGroup = min(SORT_SIZE, NumElements.x - (Gid.x * SORT_SIZE));
	
    // Load shared data
    int i;
	[unroll]
    for (i = 0; i < 2 * ITERATIONS; ++i)
    {
        if (GI + i * NUM_THREADS < numElementsInThreadGroup)
            LDS[LocalBaseIndex + i * NUM_THREADS] = Data[GlobalBaseIndex + i * NUM_THREADS];
    }
    GroupMemoryBarrierWithGroupSync();
    
	// Bitonic sort
    for (unsigned int MergeSize = 2; MergeSize <= SORT_SIZE; MergeSize = MergeSize * 2)
    {
        for (int MergeSubSize = MergeSize >> 1; MergeSubSize > 0; MergeSubSize = MergeSubSize >> 1)
        {
			[unroll]
            for (i = 0; i < ITERATIONS; ++i)
            {
                int tmp_index = GI + NUM_THREADS * i;
                int index_low = tmp_index & (MergeSubSize - 1);
                int index_high = 2 * (tmp_index - index_low);
                int index = index_high + index_low;

                unsigned int SwapElem = MergeSubSize == MergeSize >> 1 ? index_high + (2 * MergeSubSize - 1) - index_low : index_high + MergeSubSize + index_low;
                if (SwapElem < numElementsInThreadGroup)
                {
                    float2 a = LDS[index];
                    float2 b = LDS[SwapElem];

                    if (a.x > b.x)
                    {
                        LDS[index] = b;
                        LDS[SwapElem] = a;
                    }
                }
                GroupMemoryBarrierWithGroupSync();
            }
        }
    }
    
    // Store shared data
	[unroll]
    for (i = 0; i < 2 * ITERATIONS; ++i)
    {
        if (GI + i * NUM_THREADS < numElementsInThreadGroup)
            Data[GlobalBaseIndex + i * NUM_THREADS] = LDS[LocalBaseIndex + i * NUM_THREADS];
    }
}