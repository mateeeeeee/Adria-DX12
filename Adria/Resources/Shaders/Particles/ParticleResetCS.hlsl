#include "../Util/RootSignatures.hlsli"
#include "ParticleGlobals.hlsli"

RWStructuredBuffer<GPUParticlePartA>	ParticleBufferA		: register(u0);
RWStructuredBuffer<GPUParticlePartB>	ParticleBufferB		: register(u1);

[RootSignature(Reset_RS)]
[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	ParticleBufferA[id.x] = (GPUParticlePartA)0;
	ParticleBufferB[id.x] = (GPUParticlePartB)0;
}