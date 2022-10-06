#include "ExposureUtil.hlsli"
#include "../Util/RootSignatures.hlsli"



[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID )
{
	Texture2D<float2> avgLuminanceTexture = ResourceDescriptorHeap[c_avgLuminanceTexture];
	float avgLuminance = avgLuminanceTexture.Load(uint2(0, 0)).x;

	float EV100 = ComputeEV100(max(avgLuminance, 0.001));

	RWTexture2D<float> previousEV100Texture = ResourceDescriptorHeap[c_previousEV100Texture];
	float previousEV100 = previousEV100Texture[uint2(0, 0)];

	//eye adaption
	EV100 = previousEV100 + (EV100 - previousEV100) * (1 - exp(-SceneCB.frameTime * c_adaptionSpeed));
	previousEV100Texture[uint2(0, 0)] = EV100;

	const float exposureCompensation = 0.5f;
	float exposure = ComputeExposure(EV100 - exposureCompensation);
	RWTexture2D<float> exposureTexture = ResourceDescriptorHeap[c_exposureTexture];
	exposureTexture[uint2(0, 0)] = exposure;
}