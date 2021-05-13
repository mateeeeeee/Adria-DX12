#include "../Globals/GlobalsGS.hlsli"

struct VertexOut
{
    float4 pos : SV_POSITION;
    nointerpolation uint vid : VERTEXID;
};

struct GeometryOut
{
    float4 pos : SV_POSITION;
    float3 texPos : TEXCOORD0; // texture coordinates (xy) + offset(z)
    nointerpolation uint sel : TEXCOORD1; // texture selector
    nointerpolation float4 opa : TEXCOORD2; // opacity + padding
};


Texture2D lens0         : register(t0);
Texture2D lens1         : register(t1);
Texture2D lens2         : register(t2);
Texture2D lens3         : register(t3);
Texture2D lens4         : register(t4);
Texture2D lens5         : register(t5);
Texture2D lens6         : register(t6);
Texture2D depth_texture : register(t7);

SamplerState point_clamp_sampler : register(s0);

// Append a screen space quad to the output stream:
inline void append(inout TriangleStream<GeometryOut> triStream, GeometryOut p1, uint selector, float2 posMod, float2 size)
{
    float2 pos = (light_cbuf.current_light.ss_position.xy - 0.5) * float2(2, -2);
    float2 moddedPos = pos * posMod;
    float dis = distance(pos, moddedPos);

    p1.pos.xy = moddedPos + float2(-size.x, -size.y);
    p1.texPos.z = dis;
    p1.sel = selector;
    p1.texPos.xy = float2(0, 0);
    triStream.Append(p1);
	
    p1.pos.xy = moddedPos + float2(-size.x, size.y);
    p1.texPos.xy = float2(0, 1);
    triStream.Append(p1);

    p1.pos.xy = moddedPos + float2(size.x, -size.y);
    p1.texPos.xy = float2(1, 0);
    triStream.Append(p1);
	
    p1.pos.xy = moddedPos + float2(size.x, size.y);
    p1.texPos.xy = float2(1, 1);
    triStream.Append(p1);
}


static const float mods[] = { 1, 0.55, 0.4, 0.1, -0.1, -0.3, -0.5 };


[maxvertexcount(4)]
void main(
    point VertexOut p[1], inout TriangleStream<GeometryOut> triStream)
{
    GeometryOut p1 = (GeometryOut) 0;

	// Determine flare size from texture dimensions
    float2 flareSize = float2(256, 256);
	[branch]
    switch (p[0].vid)
    {
        case 0:
            lens0.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 1:
            lens1.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 2:
            lens2.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 3:
            lens3.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 4:
            lens4.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 5:
            lens5.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 6:
            lens6.GetDimensions(flareSize.x, flareSize.y);
            break;
        default:
            break;
    };
    
    uint width, height, levels;
    depth_texture.GetDimensions(0, width, height, levels);
    
    float2 ScreenResolution = float2(width, height);
   
    flareSize /= ScreenResolution;

    float referenceDepth = saturate(light_cbuf.current_light.ss_position.z);

	// determine the flare opacity:
	// These values work well for me, but should be tweakable
    const float2 step = 1.0f / ScreenResolution;
    const float2 range = 10.5f * step;
    float samples = 0.0f;
    float accdepth = 0.0f;
    for (float y = -range.y; y <= range.y; y += step.y)
    {
        for (float x = -range.x; x <= range.x; x += step.x)
        {
            samples += 1.0f;
            accdepth += depth_texture.SampleLevel(point_clamp_sampler, light_cbuf.current_light.ss_position.xy + float2(x, y), 0).r >= referenceDepth - 0.001 ? 1 : 0;
        }
    }
    accdepth /= samples;

    p1.pos = float4(0, 0, 0, 1);
    p1.opa = float4(accdepth, 0, 0, 0);

	 //Make a new flare if it is at least partially visible:
	[branch]
    if (accdepth > 0)
    {
      append(triStream, p1, p[0].vid, mods[p[0].vid], flareSize);
    }
}