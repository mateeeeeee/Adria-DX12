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

ConstantBuffer<ExposureConstants> ExposurePassCB : register(b1);

[numthreads(1, 1, 1)]
void ExposureCS()
{
	RWTexture2D<float> 			previousEV100Texture = ResourceDescriptorHeap[ExposurePassCB.previousEVIdx];
	RWTexture2D<float>			exposureTexture = ResourceDescriptorHeap[ExposurePassCB.exposureIdx];
	Texture2D					averageLuminanceTexture = ResourceDescriptorHeap[ExposurePassCB.luminanceIdx];

	float avgLuminance = averageLuminanceTexture.Load(uint3(0, 0, 0)).x;
	float EV100 = ComputeEV100(max(avgLuminance, 0.001));

	float previousEV100 = previousEV100Texture[uint2(0, 0)];

	EV100 = previousEV100 + (EV100 - previousEV100) * (1 - exp(-ExposurePassCB.frameTime * ExposurePassCB.adaptionSpeed));
	previousEV100Texture[uint2(0, 0)] = EV100;

	float exposure = ComputeExposure(EV100 - ExposurePassCB.exposureCompensation);
	exposureTexture[uint2(0, 0)] = exposure;
}