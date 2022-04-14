#include "../Globals/GlobalsPS.hlsli"
#include "../Util/RootSignatures.hlsli"


Texture2D normalMetallicTx      : register(t0);
Texture2D sceneTx               : register(t1);
Texture2D<float> depthTx        : register(t2);

SamplerState point_clamp_sampler    : register(s0);
SamplerState linear_clamp_sampler   : register(s1);
SamplerState linear_border_sampler  : register(s2);


static const int g_iMaxSteps = 16;
static const int g_iNumBinarySearchSteps = 16;

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
};


float4 SSRBinarySearch(float3 vDir, inout float3 vHitCoord)
{
    float fDepth;

    for (int i = 0; i < g_iNumBinarySearchSteps; i++)
    {
        float4 vProjectedCoord = mul(float4(vHitCoord, 1.0f), frame_cbuf.projection);
        vProjectedCoord.xy /= vProjectedCoord.w;
        vProjectedCoord.xy = vProjectedCoord.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

       //linearize depth here
        fDepth = depthTx.SampleLevel(point_clamp_sampler, vProjectedCoord.xy, 0);
        float3 fPositionVS = GetPositionVS(vProjectedCoord.xy, fDepth);
        float fDepthDiff = vHitCoord.z - fPositionVS.z;

        if (fDepthDiff <= 0.0f)
            vHitCoord += vDir;

        vDir *= 0.5f;
        vHitCoord -= vDir;
    }

    float4 vProjectedCoord = mul(float4(vHitCoord, 1.0f), frame_cbuf.projection);
    vProjectedCoord.xy /= vProjectedCoord.w;
    vProjectedCoord.xy = vProjectedCoord.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

   //linearize depth here
    fDepth = depthTx.SampleLevel(point_clamp_sampler, vProjectedCoord.xy, 0);
    float3 fPositionVS = GetPositionVS(vProjectedCoord.xy, fDepth);
    float fDepthDiff = vHitCoord.z - fPositionVS.z;

    return float4(vProjectedCoord.xy, fDepth, abs(fDepthDiff) < postprocess_cbuf.ssr_ray_hit_threshold ? 1.0f : 0.0f);
}

float4 SSRRayMarch(float3 vDir, inout float3 vHitCoord)
{
    float fDepth;

    for (int i = 0; i < g_iMaxSteps; i++)
    {
        vHitCoord += vDir;

        float4 vProjectedCoord = mul(float4(vHitCoord, 1.0f), frame_cbuf.projection);
        vProjectedCoord.xy /= vProjectedCoord.w;
        vProjectedCoord.xy = vProjectedCoord.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

		//linearize depth here
        fDepth = depthTx.SampleLevel(point_clamp_sampler, vProjectedCoord.xy, 0); 

        float3 fPositionVS = GetPositionVS(vProjectedCoord.xy, fDepth);

        float fDepthDiff = vHitCoord.z - fPositionVS.z;

		[branch]
        if (fDepthDiff > 0.0f)
        {
            return SSRBinarySearch(vDir, vHitCoord);
        }

        vDir *= postprocess_cbuf.ssr_ray_step;

    }
    
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}


bool bInsideScreen(in float2 vCoord)
{
    return !(vCoord.x < 0 || vCoord.x > 1 || vCoord.y < 0 || vCoord.y > 1);
}

[RootSignature(Ssr_RS)]
float4 main(VertexOut pin) : SV_TARGET
{
	
    float4 NormalMetallic = normalMetallicTx.Sample(linear_border_sampler, pin.Tex); //zamijeni kasnije sa pointSamplerom
    
    float metallic = NormalMetallic.a;
    
    float4 scene_color = sceneTx.SampleLevel(linear_clamp_sampler, pin.Tex, 0);
    
    if (metallic < 0.01f)
        return scene_color;
    
    float3 Normal = NormalMetallic.rgb;
    Normal = 2 * Normal - 1.0;
    Normal = normalize(Normal);
    

    float depth = depthTx.Sample(linear_clamp_sampler, pin.Tex);
    
    float3 Position = GetPositionVS(pin.Tex, depth);
    
    float3 ReflectDir = normalize(reflect(Position, Normal));
	
	//Raycast
    float3 HitPos = Position;
	
    float4 vCoords = SSRRayMarch(ReflectDir, HitPos);
    

    float2 vCoordsEdgeFact = float2(1, 1) - pow(saturate(abs(vCoords.xy - float2(0.5f, 0.5f)) * 2), 8);
    float fScreenEdgeFactor = saturate(min(vCoordsEdgeFact.x, vCoordsEdgeFact.y));
	
    float reflectionIntensity =
		saturate(
			fScreenEdgeFactor * // screen fade
			saturate(ReflectDir.z) // camera facing fade
			* (vCoords.w)// / 2 + 0.5f) // rayhit binary fade
			);


    float3 reflectionColor = reflectionIntensity * sceneTx.SampleLevel(linear_clamp_sampler, vCoords.xy, 0).rgb;

    //return lerp(scene_color, float4(reflectionColor, 1.0f), metallic);
    
    return scene_color + metallic * max(0, float4(reflectionColor, 1.0f));

}


