Texture2D<float4> SrcTexture : register(t0);
RWTexture2D<float4> DstTexture : register(u0);
SamplerState BilinearClamp : register(s0);

cbuffer CB : register(b0)
{
    float2 TexelSize; // 1.0 / destination dimension
}

float4 Mip(uint2 coord)
{
    float2 uv = (coord.xy + 0.5) * TexelSize;
    return SrcTexture.SampleLevel(BilinearClamp, uv, 0);
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	//Write the final color into the destination texture.
    DstTexture[DTid.xy] = Mip(DTid.xy);
}

