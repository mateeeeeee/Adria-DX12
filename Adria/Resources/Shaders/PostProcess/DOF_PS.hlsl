#include "../Globals/GlobalsPS.hlsli"
#include "../Util/RootSignatures.hlsli"


Texture2D scene_texture : register(t0);

Texture2D blurred_texture : register(t1);

Texture2D<float> depth_map : register(t2);

SamplerState linear_wrap_sampler : register(s0);


float BlurFactor(in float depth, in float4 DOF)
{
    float f0 = 1.0f - saturate((depth - DOF.x) / max(DOF.y - DOF.x, 0.01f));
    float f1 = saturate((depth - DOF.z) / max(DOF.w - DOF.z, 0.01f));
    float blur = saturate(f0 + f1);

    return blur;
}

float BlurFactor2(in float depth, in float2 DOF)
{
    return saturate((depth - DOF.x) / (DOF.y - DOF.x));
}

float3 DistanceDOF(float3 colorFocus, float3 colorBlurred, float depth)
{
    float blurFactor = BlurFactor(depth, postprocess_cbuf.dof_params);
    return lerp(colorFocus, colorBlurred, blurFactor);
}

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};


[RootSignature(DepthOfField_RS)]
float4 main(VertexOut pin) : SV_TARGET
{
    
    float4 color = scene_texture.Sample(linear_wrap_sampler, pin.Tex);
    
    float depth = depth_map.Sample(linear_wrap_sampler, pin.Tex);
    

    // Get the blurred color from the blurred down scaled HDR texture
    float3 colorBlurred = blurred_texture.Sample(linear_wrap_sampler, pin.Tex).xyz;

	// Convert the full resulotion depth to linear depth
    depth = ConvertZToLinearDepth(depth);

	// Compute the distance DOF color
    color = float4(DistanceDOF(color.xyz, colorBlurred, depth), 1.0);

    return color;

}