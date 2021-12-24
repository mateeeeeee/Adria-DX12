#include "../Util/RootSignatures.hlsli"

#define SORT_SIZE       512
#define NUM_THREADS		(SORT_SIZE/2)
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
    int4 tgp;

    tgp.x = Gid.x * 256;
    tgp.y = 0;
    tgp.z = NumElements.x;
    tgp.w = min(512, max(0, NumElements.x - Gid.x * 512));

    int GlobalBaseIndex = tgp.y + tgp.x * 2 + GTid.x;
    int LocalBaseIndex = GI;
    int i;

    // Load shared data
	[unroll]
    for (i = 0; i < 2; ++i)
    {
        if (GI + i * NUM_THREADS < tgp.w)
            LDS[LocalBaseIndex + i * NUM_THREADS] = Data[GlobalBaseIndex + i * NUM_THREADS];
    }
    GroupMemoryBarrierWithGroupSync();

	// sort threadgroup shared memory
    for (int MergeSubSize = SORT_SIZE >> 1; MergeSubSize > 0; MergeSubSize = MergeSubSize >> 1)
    {
        int tmp_index = GI;
        int index_low = tmp_index & (MergeSubSize - 1);
        int index_high = 2 * (tmp_index - index_low);
        int index = index_high + index_low;

        unsigned int SwapElem = index_high + MergeSubSize + index_low;

        if (SwapElem < tgp.w)
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
    
    // Store shared data
	[unroll]
    for (i = 0; i < 2; ++i)
    {
        if (GI + i * NUM_THREADS < tgp.w)
            Data[GlobalBaseIndex + i * NUM_THREADS] = LDS[LocalBaseIndex + i * NUM_THREADS];
    }
}
