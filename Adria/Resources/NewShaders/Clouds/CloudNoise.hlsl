#include "../Noise.hlsli"

struct CloudNoiseConstants
{
	float resolutionInv;
	uint frequency;
	uint outputIdx;
};
ConstantBuffer<CloudNoiseConstants> PassCB : register(b1);

[numthreads(8, 8, 8)]
void CloudShapeCS(uint3 threadId : SV_DispatchThreadID)
{
	float3 uvw = (threadId.xyz + 0.5f) * (float)PassCB.resolutionInv;

	float perlin = lerp(1.0f, PerlinFBM(uvw, 4.0f, 7), 0.5f);
	perlin = abs(perlin * 2.0f - 1.0f);

	float4 noise = 0;
	noise.y = WorleyFBM(uvw, PassCB.frequency);
	noise.z = WorleyFBM(uvw, PassCB.frequency * 2);
	noise.w = WorleyFBM(uvw, PassCB.frequency * 4);
	noise.x = Remap(perlin, 0.0f, 1.0f, noise.y, 1.0f);

	RWTexture3D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	outputTx[threadId.xyz] = noise;
}

[numthreads(8, 8, 8)]
void CloudDetailCS(uint3 threadId : SV_DispatchThreadID)
{
	float3 uvw = (threadId.xyz + 0.5f) * (float)PassCB.resolutionInv;

	float4 noise = 0;
	noise.x = WorleyFBM(uvw, PassCB.frequency);
	noise.y = WorleyFBM(uvw, PassCB.frequency * 2);
	noise.z = WorleyFBM(uvw, PassCB.frequency * 4);
	noise.w = WorleyFBM(uvw, PassCB.frequency * 8);

	RWTexture3D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	outputTx[threadId.xyz] = noise;
}


float GetRatio(float value, float minValue, float maxValue)
{
	return (value - minValue) / (maxValue - minValue);
}

[numthreads(8, 8, 8)]
void CloudTypeCS(uint3 threadId : SV_DispatchThreadID)
{
	float3 uvw = (threadId.xyz + 0.5f) * PassCB.resolutionInv;
	float cloudType = uvw.x;
	float height = uvw.y;

	const float4 stratoGradient = float4(0.0f, 0.07f, 0.08f, 0.15f);
	const float4 stratoCumulusGradient = float4(0.0f, 0.2f, 0.42f, 0.6f);
	const float4 culumulusGradient = float4(0.0f, 0.08f, 0.75f, 0.98f);

	float a = 1.0f - saturate(cloudType * 2.0f);
	float b = 1.0f - abs(cloudType - 0.5f) * 2.0f;
	float c = saturate(cloudType - 0.5f) * 2.0f;
	float4 gradient = stratoGradient * a + stratoCumulusGradient * b + culumulusGradient * c;

	float v1 = saturate(GetRatio(height, gradient.x, gradient.y));
	float v2 = saturate(GetRatio(height, gradient.w, gradient.z));
	float v = v1 * v2;

	RWTexture2D<float4> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];
	outputTx[threadId.xy] = v;
}

