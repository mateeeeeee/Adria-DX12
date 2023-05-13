#include "../CommonResources.hlsli"

struct InitializeHZBConstants
{
	uint depthIdx;
	uint hzbIdx;
	float2 hzbDimsInv;
};

ConstantBuffer<InitializeHZBConstants> PassCB : register(b0);

[numthreads(16, 16, 1)]
void InitializeHZB_CS(uint3 threadId : SV_DispatchThreadID)
{
	RWTexture2D<float> hzbTx = ResourceDescriptorHeap[PassCB.hzbIdx];
	Texture2D<float> depthTx = ResourceDescriptorHeap[PassCB.depthIdx];

	float2 uv = ((float2)threadId.xy + 0.5f) * PassCB.hzbDimsInv;
	float4 depths = depthTx.Gather(PointClampSampler, uv);
	float  maxDepth = max(max(max(depths.x, depths.y), depths.z), depths.w);
	hzbTx[threadId.xy] = maxDepth;
}

///////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

struct SpdConstants
{
	uint mips;
	uint numWorkGroups;
	uint2 workGroupOffset;
};

ConstantBuffer<SpdConstants> spdConstants : register(b1);

struct SPDIndices
{
	uint4	dstIdx[12];
	uint	spdGlobalAtomicIdx;
};
ConstantBuffer<SPDIndices> spdIndices : register(b2);


//#define globallycoherent

#define A_GPU 1
#define A_HLSL 1
#include "../SPD/ffx_a.h"

groupshared AF1 spdIntermediate[16][16];
groupshared AU1 spdCounter;

AF4 SpdLoadSourceImage(ASU2 tex, AU1 slice)
{
	RWTexture2D<float> destination0 = ResourceDescriptorHeap[spdIndices.dstIdx[0].x];
	return destination0[tex];
}
AF4 SpdLoad(ASU2 tex, AU1 slice)
{
	globallycoherent RWTexture2D<float> destination6 = ResourceDescriptorHeap[spdIndices.dstIdx[6].x];
	return destination6[tex];
}
void SpdStore(ASU2 pix, AF4 value, AU1 mip, AU1 slice)
{
	if (mip == 5)
	{
		globallycoherent RWTexture2D<float> destination6 = ResourceDescriptorHeap[spdIndices.dstIdx[6].x];
		destination6[pix] = value.x;
		return;
	}
	RWTexture2D<float> destination = ResourceDescriptorHeap[spdIndices.dstIdx[mip + 1].x];
	destination[pix] = value.x;
}

void SpdIncreaseAtomicCounter(AU1 slice)
{
	globallycoherent RWBuffer<uint> spdGlobalAtomic = ResourceDescriptorHeap[spdIndices.spdGlobalAtomicIdx];
	InterlockedAdd(spdGlobalAtomic[slice], 1, spdCounter);
}
AU1 SpdGetAtomicCounter()
{
	globallycoherent RWBuffer<uint> spdGlobalAtomic = ResourceDescriptorHeap[spdIndices.spdGlobalAtomicIdx];
	return spdCounter;
}
void SpdResetAtomicCounter(AU1 slice)
{
	globallycoherent RWBuffer<uint> spdGlobalAtomic = ResourceDescriptorHeap[spdIndices.spdGlobalAtomicIdx];
	spdGlobalAtomic[slice] = 0;
}

AF4 SpdLoadIntermediate(AU1 x, AU1 y) { return spdIntermediate[x][y]; }
void SpdStoreIntermediate(AU1 x, AU1 y, AF4 value) { spdIntermediate[x][y] = value.x; }

AF4 SpdReduce4(AF4 v0, AF4 v1, AF4 v2, AF4 v3)
{
	return max(v0, max(v1, max(v2, v3)));
}

#include "../SPD/ffx_spd.h"

[numthreads(256, 1, 1)]
void HZBMipsCS(uint3 workGroupId : SV_GroupID, uint localThreadIndex : SV_GroupIndex)
{
	SpdDownsample(
		AU2(workGroupId.xy),
		AU1(localThreadIndex),
		AU1(spdConstants.mips),
		AU1(spdConstants.numWorkGroups),
		AU1(workGroupId.z)
	);
}

