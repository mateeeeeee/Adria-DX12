#include "CommonResources.hlsli"
#include "Constants.hlsli"

#define BLOCK_SIZE 16

static const float g = 9.81f;
static const float KM = 370.0f;
static const float CM = 0.23f;


float Omega(float k)
{
	return sqrt(g * k * (1.f + ((k * k) / (KM * KM))));
}
float Square(float x)
{
	return x * x;
}
float2 MultiplyComplex(float2 a, float2 b)
{
	return float2(a.x * b.x - a.y * b.y, a.y * b.x + a.x * b.y);
}
float2 MultiplyByI(float2 z)
{
	return float2(-z.y, z.x);
}

struct InitialSpectrumConstants
{
	float fftResolution;
	float oceanSize;
	uint  outputIdx;
};
ConstantBuffer<InitialSpectrumConstants> PassCB : register(b1);

struct CSInput
{
	uint3 GroupId : SV_GroupID;
	uint3 GroupThreadId : SV_GroupThreadID;
	uint3 DispatchThreadId : SV_DispatchThreadID;
	uint GroupIndex : SV_GroupIndex;
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void InitialSpectrumCS(CSInput input)
{
	RWTexture2D<float> outputTx = ResourceDescriptorHeap[PassCB.outputIdx];

	uint2 pixelCoord = input.DispatchThreadId.xy;
	float n = (pixelCoord.x < 0.5f * uint(PassCB.fftResolution)) ? pixelCoord.x : pixelCoord.x - PassCB.fftResolution;
	float m = (pixelCoord.y < 0.5f * uint(PassCB.fftResolution)) ? pixelCoord.y : pixelCoord.y - PassCB.fftResolution;

	float2 waveVector = (2.f * M_PI * float2(n, m)) / PassCB.oceanSize;
	float k = length(waveVector);
	if (waveVector.x == 0.0 && waveVector.y == 0.0)
	{
		outputTx[pixelCoord] = 0.0f;
		return;
	}
	float2 windDir = FrameCB.windParams.xz * FrameCB.windParams.w;

	float U10 = length(float2(windDir.x, windDir.y));

	float omega = 0.84f;
	float kp = g * Square(omega / U10);
	float c = Omega(k) / k;
	float cp = Omega(kp) / kp;

	float Lpm = exp(-1.25 * Square(kp / k));
	float gamma = 1.7;
	float sigma = 0.08 * (1.0 + 4.0 * pow(omega, -3.0));
	float Gamma = exp(-Square(sqrt(k / kp) - 1.0) / 2.0 * Square(sigma));
	float Jp = pow(gamma, Gamma);
	float Fp = Lpm * Jp * exp(-omega / sqrt(10.0) * (sqrt(k / kp) - 1.0));
	float alphap = 0.006 * sqrt(omega);
	float Bl = 0.5 * alphap * cp / c * Fp;

	float z0 = 0.000037 * Square(U10) / g * pow(U10 / cp, 0.9);
	float uStar = 0.41 * U10 / log(10.0 / z0);
	float alpham = 0.01 * ((uStar < CM) ? (1.0 + log(uStar / CM)) : (1.0 + 3.0 * log(uStar / CM)));
	float Fm = exp(-0.25 * Square(k / KM - 1.0));
	float Bh = 0.5 * alpham * CM / c * Fm * Lpm;

	float a0 = log(2.0) / 4.0;
	float am = 0.13 * uStar / CM;
	float Delta = tanh(a0 + 4.0 * pow(c / cp, 2.5) + am * pow(CM / c, 2.5));

	float cosPhi = dot(normalize(float2(windDir.x, windDir.y)), normalize(waveVector));

	float S = (1.0 / (2.0 * M_PI)) * pow(k, -4.0) * (Bl + Bh) * (1.0 + Delta * (2.0 * cosPhi * cosPhi - 1.0));

	float dk = 2.0 * M_PI / PassCB.oceanSize;
	float h = sqrt(S / 2.0) * dk;

	outputTx[pixelCoord] = h;
}
