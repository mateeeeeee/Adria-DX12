#include "../Globals/GlobalsPS.hlsli"


Texture2D normalTx : register(t1);
Texture2D<float> depthTx : register(t2);
Texture2D noiseTx : register(t3);  

SamplerState linear_border_sampler : register(s0);
SamplerState point_wrap_sampler : register(s1);



struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};


float4 main(VertexOut pin) : SV_TARGET
{
	
    float3 Normal = normalTx.Sample(linear_border_sampler, pin.Tex).rgb; //zamijeni kasnije sa pointSamplerom
    Normal = 2 * Normal - 1.0;
    Normal = normalize(Normal);
   
    
    float depth = depthTx.Sample(linear_border_sampler, pin.Tex);
    
    float3 Position = GetPositionVS(pin.Tex, depth);

    float3 RandomVec = normalize(2 * noiseTx.Sample(point_wrap_sampler, pin.Tex * postprocess_cbuf.noise_scale).xyz - 1); //use wrap sampler

    float3 Tangent = normalize(RandomVec - Normal * dot(RandomVec, Normal));
    float3 Bitangent = cross(Normal, Tangent);
    float3x3 TBN = float3x3(Tangent, Bitangent, Normal);
    
    float occlusion = 0.0;
    
    [unroll(SSAO_KERNEL_SIZE)]
    for (int i = 0; i < SSAO_KERNEL_SIZE; ++i)
    {
        float3 sampleDir = mul(postprocess_cbuf.samples[i].xyz, TBN); // from tangent to view-space
        float3 samplePos = Position + sampleDir * postprocess_cbuf.ssao_radius;
        
        //float nDotS = max(dot(Normal, sampleDir), 0);

        float4 offset = float4(samplePos, 1.0);
        offset = mul(offset, frame_cbuf.projection); 
        
        offset.xy = ((offset.xy / offset.w) * float2(1.0f, -1.0f)) * 0.5f + 0.5f; 
        
        float sampleDepth = depthTx.Sample(linear_border_sampler, offset.xy);
        
       sampleDepth = GetPositionVS(offset.xy, sampleDepth).z;
        
        // range check & accumulate
        float rangeCheck = smoothstep(0.0, 1.0, postprocess_cbuf.ssao_radius / abs(Position.z - sampleDepth));
        occlusion += rangeCheck * step(sampleDepth, samplePos.z - 0.01); // * nDotS;
    }

    occlusion = 1.0 - (occlusion / SSAO_KERNEL_SIZE); //
    
    return pow(abs(occlusion), postprocess_cbuf.ssao_power);

} 