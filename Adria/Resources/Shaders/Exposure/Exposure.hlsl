#include "ExposureUtil.hlsli"

struct ExposureConstants
{
	float adaptionSpeed;
	float exposureCompensation;
	float frameTime;
	uint  previousEVIdx;
	uint  exposureIdx;
	uint  luminanceIdx;
};

ConstantBuffer<ExposureConstants> PassCB : register(b1);

[numthreads(1, 1, 1)]
void ExposureCS(uint3 DTid : SV_DispatchThreadID)
{
	RWTexture2D<float> 			PreviousEV100 = ResourceDescriptorHeap[PassCB.previousEVIdx];
	RWTexture2D<float>			Exposure = ResourceDescriptorHeap[PassCB.exposureIdx];
	Texture2D					AvgLuminanceTexture = ResourceDescriptorHeap[PassCB.luminanceIdx];

	float avgLuminance = AvgLuminanceTexture.Load(uint3(0, 0, 0)).x;
	float EV100 = ComputeEV100(max(avgLuminance, 0.001));

	float previousEV100 = PreviousEV100[uint2(0, 0)];

	EV100 = previousEV100 + (EV100 - previousEV100) * (1 - exp(-PassCB.frameTime * PassCB.adaptionSpeed));
	PreviousEV100[uint2(0, 0)] = EV100;

	float exposure = ComputeExposure(EV100 - PassCB.exposureCompensation);
	Exposure[uint2(0, 0)] = exposure;
}