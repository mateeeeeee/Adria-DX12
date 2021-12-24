#include "../Util/RootSignatures.hlsli"
#include "ParticleGlobals.hlsli"

Texture2D								RandomBuffer			: register(t0);
RWStructuredBuffer<GPUParticlePartA>	ParticleBufferA		    : register(u0);
RWStructuredBuffer<GPUParticlePartB>	ParticleBufferB		    : register(u1);
ConsumeStructuredBuffer<uint>			DeadListToAllocFrom	    : register(u2);

SamplerState linear_wrap_sampler : register(s0);

[RootSignature(Emit_RS)]
[numthreads(1024, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	if (id.x < NumDeadParticles && id.x < MaxParticlesThisFrame)
	{
		GPUParticlePartA pa = (GPUParticlePartA)0;
		GPUParticlePartB pb = (GPUParticlePartB)0;

		float2 uv = float2(id.x / 1024.0, ElapsedTime);
		float3 randomValues0 = RandomBuffer.SampleLevel(linear_wrap_sampler, uv, 0).xyz;

        float2 uv2 = float2((id.x + 1) / 1024.0, ElapsedTime);
        float3 randomValues1 = RandomBuffer.SampleLevel(linear_wrap_sampler, uv2, 0).xyz;

        pa.TintAndAlpha = float4(1, 1, 1, 1);
        pa.Rotation = 0;
        pa.IsSleeping = 0;
		
		float velocityMagnitude = length(EmitterVelocity.xyz);
        pb.Position = EmitterPosition.xyz + (randomValues0.xyz * PositionVariance.xyz);
		pb.Mass = Mass;
        pb.Velocity = EmitterVelocity.xyz + (randomValues1.xyz * velocityMagnitude * VelocityVariance);
		pb.Lifespan = ParticleLifeSpan;
		pb.Age = pb.Lifespan;
		pb.StartSize = StartSize;
		pb.EndSize = EndSize;

		uint index = DeadListToAllocFrom.Consume();

		ParticleBufferA[index] = pa;
		ParticleBufferB[index] = pb;
	}
}