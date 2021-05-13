
Texture2D    g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct VSInput
{
    float3 pos : POS;
    float2 uv  : TEX;
};

struct PerFrame
{
    row_major matrix view;
    
    row_major matrix projection;
    
    row_major matrix viewprojection;
    
    row_major matrix inverse_view;
    
    row_major matrix inverse_projection;
};


ConstantBuffer<PerFrame> frame_data : register(b0);


struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEX;
};

PSInput vs_main(VSInput input)
{
    PSInput result;

    result.position = mul(float4(input.pos, 1.0f), frame_data.viewprojection);
    result.uv = input.pos.xy;
    return result;
}

float4 ps_main(PSInput input) : SV_TARGET
{
    float4 texColor = g_texture.Sample(g_sampler, input.uv);
    return texColor + float4(0.2f, 0.0f, 0.0f, 1.0f);

}