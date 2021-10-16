#include "../Util/RootSignatures.hlsli"

#define FFT_WORK_GROUP_DIM 256

Texture2D<float4>   input   : register(t0);
RWTexture2D<float4> output  : register(u0);


cbuffer FFTCBuffer : register(b10)
{
    int seq_count;
    int subseq_count;
}

float2 MultiplyComplex(float2 a, float2 b)
{
    return float2(a[0] * b[0] - a[1] * b[1], a[1] * b[0] + a[0] * b[1]);
}

float4 ButterflyOperation(float2 a, float2 b, float2 twiddle)
{
    float2 twiddle_b = MultiplyComplex(twiddle, b);
    float4 result = float4(a + twiddle_b, a - twiddle_b);
    return result;
}


[RootSignature(FFT_RS)]
[numthreads(FFT_WORK_GROUP_DIM, 1, 1)]
void cs_main(uint3 group_id : SV_GroupID, uint3 group_thread_id : SV_GroupThreadID)
{
    uint2 pixel_coord = uint2(group_thread_id.x, group_id.x);

    int thread_count = int(seq_count * 0.5f);
    int thread_idx = pixel_coord.x;

    int in_idx = thread_idx & (subseq_count - 1);
    int out_idx = ((thread_idx - in_idx) << 1) + in_idx;

    float angle = -PI * (float(in_idx) / float(subseq_count));
    float2 twiddle = float2(cos(angle), sin(angle));
    
    float4 a = input.Load(uint3(pixel_coord, 0));
    float4 b = input.Load(uint3(pixel_coord.x + thread_count, pixel_coord.y, 0));

	// Transforming two complex sequences independently and simultaneously

    float4 result0 = ButterflyOperation(a.xy, b.xy, twiddle);
    float4 result1 = ButterflyOperation(a.zw, b.zw, twiddle);
    
    output[int2(out_idx, pixel_coord.y)] = float4(result0.xy, result1.xy);
    output[int2(out_idx + subseq_count, pixel_coord.y)] = float4(result0.zw, result1.zw);
}