#ifndef GPU_DRIVEN_RENDERING_INCLUDED
#define GPU_DRIVEN_RENDERING_INCLUDED
#include "../CommonResources.hlsli"

//credits: https://github.com/simco50/D3D12_Research

//move to some common header file
template<typename T>
T min(T a, T b, T c)
{
	return min(min(a, b), c);
}
template<typename T>
T min(T a, T b, T c, T d)
{
	return min(min(a, b), min(c,d));
}

template<typename T>
T max(T a, T b, T c)
{
	return max(max(a, b), c);
}
template<typename T>
T max(T a, T b, T c, T d)
{
	return max(max(a, b), max(c,d));
}

struct FrustumCullData
{
	bool   isVisible;
	float3 rectMin;
	float3 rectMax;
};

FrustumCullData FrustumCull(float3 aabbCenter, float3 aabbExtents, float4x4 worldToClip)
{
	FrustumCullData data = (FrustumCullData)0;
	data.isVisible = true;

	float3x4 axis;
	axis[0] = mul(float4(aabbExtents.x * 2, 0, 0, 0), worldToClip);
	axis[1] = mul(float4(0, aabbExtents.y * 2, 0, 0), worldToClip);
	axis[2] = mul(float4(0, 0, aabbExtents.z * 2, 0), worldToClip);

	float4 pos000 = mul(float4(aabbCenter - aabbExtents, 1), worldToClip);
	float4 pos100 = pos000 + axis[0];
	float4 pos010 = pos000 + axis[1];
	float4 pos110 = pos010 + axis[0];
	float4 pos001 = pos000 + axis[2];
	float4 pos101 = pos100 + axis[2];
	float4 pos011 = pos010 + axis[2];
	float4 pos111 = pos110 + axis[2];

	float minW = min(min(pos000.w, pos100.w, pos010.w),
					 min(pos110.w, pos001.w, pos101.w),
					 min(pos011.w, pos111.w));

	float maxW = max(max(pos000.w, pos100.w, pos010.w),
					 max(pos110.w, pos001.w, pos101.w),
					 max(pos011.w, pos111.w));

	// Plane inequalities
	float4 planeMins = min(
		min(float4(pos000.xy, -pos000.xy) - pos000.w, float4(pos001.xy, -pos001.xy) - pos001.w, float4(pos010.xy, -pos010.xy) - pos010.w),
		min(float4(pos100.xy, -pos100.xy) - pos100.w, float4(pos110.xy, -pos110.xy) - pos110.w, float4(pos011.xy, -pos011.xy) - pos011.w),
		min(float4(pos101.xy, -pos101.xy) - pos101.w, float4(pos111.xy, -pos111.xy) - pos111.w, float4(1, 1, 1, 1))
	);

	// Screen space AABB
	float3 ssPos000 = pos000.xyz / pos000.w;
	float3 ssPos100 = pos100.xyz / pos100.w;
	float3 ssPos010 = pos010.xyz / pos010.w;
	float3 ssPos110 = pos110.xyz / pos110.w;
	float3 ssPos001 = pos001.xyz / pos001.w;
	float3 ssPos101 = pos101.xyz / pos101.w;
	float3 ssPos011 = pos011.xyz / pos011.w;
	float3 ssPos111 = pos111.xyz / pos111.w;

	data.rectMin = min(
		min(ssPos000, ssPos100, ssPos010),
		min(ssPos110, ssPos001, ssPos101),
		min(ssPos011, ssPos111, float3(1, 1, 1))
	);

	data.rectMax = max(
		max(ssPos000, ssPos100, ssPos010),
		max(ssPos110, ssPos001, ssPos101),
		max(ssPos011, ssPos111, float3(-1, -1, -1))
	);

	data.isVisible = data.isVisible && (data.rectMax.z > 0);

	if (minW <= 0 && maxW > 0)
	{
		data.rectMin = -1;
		data.rectMax = 1;
		data.isVisible = true;
	}
	else
	{
		data.isVisible = data.isVisible && maxW > 0.0f;
	}

	data.isVisible = data.isVisible && !any(planeMins > 0.0f);

	return data;
}
FrustumCullData FrustumCull(float3 aabbCenter, float3 aabbExtents, float4x4 localToWorld, float4x4 worldToClip)
{
	// Transform bounds to world space
	aabbExtents = mul(aabbExtents, (float3x3)localToWorld);
	aabbCenter = mul(float4(aabbCenter, 1), localToWorld).xyz;
	return FrustumCull(aabbCenter, aabbExtents, worldToClip);
}

uint ComputeHZBMip(int4 rectPixels, int texelCoverage)
{
	int2 rectSize = rectPixels.zw - rectPixels.xy;
	int mipOffset = (int)log2((float)texelCoverage) - 1;
	int2 mipLevelXY = firstbithigh(rectSize);
	int mip = max(max(mipLevelXY.x, mipLevelXY.y) - mipOffset, 0);
	if (any((rectPixels.zw >> mip) - (rectPixels.xy >> mip) >= texelCoverage))
	{
		++mip;
	}
	return mip;
}
bool HZBCull(FrustumCullData cullData, Texture2D<float> hzbTexture)
{
	uint2 hzbDimensions;
	uint hzbMipCount;
	hzbTexture.GetDimensions(0, hzbDimensions.x, hzbDimensions.y, hzbMipCount);

	static const uint hzbTexelCoverage = 4;

	float4 rect = saturate(float4(cullData.rectMin.xy, cullData.rectMax.xy) * float2(0.5f, -0.5f).xyxy + 0.5f).xwzy;
	int4 rectPixels = int4(rect * hzbDimensions.xyxy + float4(0.5f, 0.5f, -0.5f, -0.5f));
	rectPixels = int4(rectPixels.xy, max(rectPixels.xy, rectPixels.zw));
	int mip = ComputeHZBMip(rectPixels, hzbTexelCoverage);
	rectPixels >>= mip;
	float2 texelSize = 1.0f / hzbDimensions * (1u << mip);

	float minDepth = cullData.rectMin.z;
	float depth = 0;

	if (hzbTexelCoverage == 4)
	{
		float4 xCoords = (min(rectPixels.x + float4(0, 1, 2, 3), rectPixels.z) + 0.5f) * texelSize.x;
		float4 yCoords = (min(rectPixels.y + float4(0, 1, 2, 3), rectPixels.w) + 0.5f) * texelSize.y;

		float depth00 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.x, yCoords.x), mip);
		float depth10 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.y, yCoords.x), mip);
		float depth20 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.z, yCoords.x), mip);
		float depth30 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.w, yCoords.x), mip);

		float depth01 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.x, yCoords.y), mip);
		float depth11 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.y, yCoords.y), mip);
		float depth21 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.z, yCoords.y), mip);
		float depth31 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.w, yCoords.y), mip);

		float depth02 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.x, yCoords.z), mip);
		float depth12 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.y, yCoords.z), mip);
		float depth22 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.z, yCoords.z), mip);
		float depth32 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.w, yCoords.z), mip);

		float depth03 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.x, yCoords.w), mip);
		float depth13 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.y, yCoords.w), mip);
		float depth23 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.z, yCoords.w), mip);
		float depth33 = hzbTexture.SampleLevel(PointClampSampler, float2(xCoords.w, yCoords.w), mip);

		depth =
			max(
				max(depth00, depth10, depth20, depth30),
				max(depth01, depth11, depth21, depth31),
				max(depth02, depth12, depth22, depth32),
				max(depth03, depth13, depth23, depth33)
			);
	}
	else if (hzbTexelCoverage == 2)
	{
		float depth00 = hzbTexture.SampleLevel(PointClampSampler, (rectPixels.xy + 0.5f) * texelSize, mip);
		float depth10 = hzbTexture.SampleLevel(PointClampSampler, (rectPixels.zy + 0.5f) * texelSize, mip);
		float depth01 = hzbTexture.SampleLevel(PointClampSampler, (rectPixels.xw + 0.5f) * texelSize, mip);
		float depth11 = hzbTexture.SampleLevel(PointClampSampler, (rectPixels.zw + 0.5f) * texelSize, mip);

		depth = max(depth00, depth10, depth01, depth11);
	}

	bool isOccluded = depth < minDepth;
	return cullData.isVisible && !isOccluded;
}

struct MeshletCandidate
{
	uint instanceID;
	uint meshletIndex;
};

#define COUNTER_TOTAL_CANDIDATE_MESHLETS 0
#define COUNTER_PHASE1_CANDIDATE_MESHLETS 1
#define COUNTER_PHASE2_CANDIDATE_MESHLETS 2
#define COUNTER_PHASE1_VISIBLE_MESHLETS 0
#define COUNTER_PHASE2_VISIBLE_MESHLETS 1

#define MAX_NUM_MESHLETS (1u << 20u)
#define MAX_NUM_INSTANCES (1u << 14u)

#define MESHLET_MAX_TRIANGLES 124
#define MESHLET_MAX_VERTICES 64

struct MeshletTriangle
{
	uint V0 : 10;
	uint V1 : 10;
	uint V2 : 10;
	uint : 2;
};

struct Meshlet
{
	float3 center;
	float  radius;
	//add cone stuff
	uint vertexCount;
	uint triangleCount;
	uint vertexOffset;
	uint triangleOffset;
};

Meshlet GetMeshletData(uint bufferIdx, uint bufferOffset, uint meshletIdx)
{
	ByteAddressBuffer meshBuffer = ResourceDescriptorHeap[bufferIdx];
	return meshBuffer.Load<Meshlet>(bufferOffset + sizeof(Meshlet) * meshletIdx);
}

#endif