#include "CommonResources.hlsli"
#include "Constants.hlsli"

#define BLOCK_SIZE 16

static const float g = 9.81f;
static const float KM = 370.0f;


float Omega(float k)
{
	return sqrt(g * k * (1.0 + k * k / KM * KM));
}
float Mod(float x, float y)
{
	return x - y * floor(x / y);
}

struct PhaseConstants
{
	float fftResolution;
	float oceanSize;
	float oceanChoppiness;
	uint  phasesIdx;
	uint  outputIdx;
};
ConstantBuffer<PhaseConstants> PassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void Phase(CSInput input)
{
	Texture2D<float4> phasesTx = ResourceDescriptorHeap[PassCB.phasesIdx];
	RWTexture2D<float4> deltaPhasesTx = ResourceDescriptorHeap[PassCB.outputIdx];

	uint2 pixelCoord = input.DispatchThreadId.xy;
	float n = (pixelCoord.x < 0.5f * uint(PassCB.fftResolution)) ? pixelCoord.x : pixelCoord.x - PassCB.fftResolution;
	float m = (pixelCoord.y < 0.5f * uint(PassCB.fftResolution)) ? pixelCoord.y : pixelCoord.y - PassCB.fftResolution;

	float2 waveVector = (2.f * M_PI * float2(n, m)) / PassCB.oceanSize;
	float k = length(waveVector);
	float slowdownFactor = 1.f;

	float deltaPhase = Omega(k) * FrameCB.deltaTime * slowdownFactor;
	float phase = phasesTx.Load(uint3(pixelCoord, 0)).r;

	phase = Mod(phase + deltaPhase, 2.f * M_PI);
	deltaPhasesTx[pixelCoord] = float4(phase, 0.f, 0.f, 0.f);
}