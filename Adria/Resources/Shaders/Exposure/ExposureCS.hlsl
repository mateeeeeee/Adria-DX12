#include "ExposureUtil.hlsli"
#include "../Util/RootSignatures.hlsli"

struct ExposureConstants
{
	float adaptionSpeed;
	float exposureCompensation;
	float frameTime;
};

ConstantBuffer<ExposureConstants> ExposureCB : register(b0);
RWTexture2D<float> 			PreviousEV100		: register(u0);
RWTexture2D<float>			Exposure			: register(u1);
Texture2D					AvgLuminanceTexture : register(t0);

[RootSignature(Exposure_RS)]
[numthreads(1, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID )
{
	float avgLuminance = AvgLuminanceTexture.Load(uint3(0, 0, 0)).x;
	float EV100 = ComputeEV100(max(avgLuminance, 0.001));

	float previousEV100 = PreviousEV100[uint2(0, 0)];

	EV100 = previousEV100 + (EV100 - previousEV100) * (1 - exp(-ExposureCB.frameTime * ExposureCB.adaptionSpeed));
	PreviousEV100[uint2(0, 0)] = EV100;

	float exposure = ComputeExposure(EV100 - ExposureCB.exposureCompensation);
	Exposure[uint2(0, 0)] = exposure;
}