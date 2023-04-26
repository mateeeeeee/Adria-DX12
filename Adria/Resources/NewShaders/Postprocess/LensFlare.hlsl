#include "../CommonResources.hlsli"

struct VertexOut
{
    float4 Pos : SV_POSITION;
    nointerpolation uint VertexId : VERTEXID;
};

struct GeometryOut
{
    float4 Pos : SV_POSITION;
    float3 TexPos : TEXCOORD0; 
    nointerpolation uint Selector : TEXCOORD1; 
    nointerpolation float4 Opacity : TEXCOORD2;
};

static const float mods[] = { 1, 0.55, 0.4, 0.1, -0.1, -0.3, -0.5 };

Texture2D lensTx0 : register(t0);
Texture2D lensTx1 : register(t1);
Texture2D lensTx2 : register(t2);
Texture2D lensTx3 : register(t3);
Texture2D lensTx4 : register(t4);
Texture2D lensTx5 : register(t5);
Texture2D lensTx6 : register(t6);

Texture2D depthTx : register(t7);

struct LensFlareConstants
{
    uint   lensIdx0;
    uint   lensIdx1;
    uint   lensIdx2;
    uint   lensIdx3;
    uint   lensIdx4;
    uint   lensIdx5;
    uint   lensIdx6;  
    uint   depthIdx;
};

struct LensFlareConstants2
{
    float3 sunScreenSpacePosition;
};

ConstantBuffer<LensFlareConstants> PassCB : register(b1);
ConstantBuffer<LensFlareConstants2> PassCB2 : register(b2);
void Append(inout TriangleStream<GeometryOut> triStream, GeometryOut p1, uint selector, float2 posMod, float2 size)
{
    float2 pos = (PassCB2.sunScreenSpacePosition.xy - 0.5f) * float2(2.0f, -2.0f);
    float2 moddedPos = pos * posMod;
    float dis = distance(pos, moddedPos);

    p1.Pos.xy = moddedPos + float2(-size.x, -size.y);
    p1.TexPos.z = dis;
    p1.Selector = selector;
    p1.TexPos.xy = float2(0, 0);
    triStream.Append(p1);
	
    p1.Pos.xy = moddedPos + float2(-size.x, size.y);
    p1.TexPos.xy = float2(0, 1);
    triStream.Append(p1);

    p1.Pos.xy = moddedPos + float2(size.x, -size.y);
    p1.TexPos.xy = float2(1, 0);
    triStream.Append(p1);
	
    p1.Pos.xy = moddedPos + float2(size.x, size.y);
    p1.TexPos.xy = float2(1, 1);
    triStream.Append(p1);
}

VertexOut LensFlareVS(uint vid : SV_VERTEXID)
{
    VertexOut o = (VertexOut)0;
    o.Pos = 0;
    o.VertexId = vid;
    return o;
}

[maxvertexcount(4)]
void LensFlareGS(point VertexOut p[1], inout TriangleStream<GeometryOut> triStream)
{
    Texture2D lensTx0 = ResourceDescriptorHeap[PassCB.lensIdx0];
    Texture2D lensTx1 = ResourceDescriptorHeap[PassCB.lensIdx1];
    Texture2D lensTx2 = ResourceDescriptorHeap[PassCB.lensIdx2];
    Texture2D lensTx3 = ResourceDescriptorHeap[PassCB.lensIdx3];
    Texture2D lensTx4 = ResourceDescriptorHeap[PassCB.lensIdx4];
    Texture2D lensTx5 = ResourceDescriptorHeap[PassCB.lensIdx5];
    Texture2D lensTx6 = ResourceDescriptorHeap[PassCB.lensIdx6];
    Texture2D depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
    
    GeometryOut p1 = (GeometryOut) 0;
    float2 flareSize = float2(256, 256);
	[branch]
    switch (p[0].VertexId)
    {
        case 0:
            lensTx0.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 1:
            lensTx1.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 2:
            lensTx2.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 3:
            lensTx3.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 4:
            lensTx4.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 5:
            lensTx5.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 6:
            lensTx6.GetDimensions(flareSize.x, flareSize.y);
            break;
        default:
            break;
    };

    float2 screenResolution = FrameCB.screenResolution;
   
    flareSize /= screenResolution;

    float referenceDepth = saturate(PassCB2.sunScreenSpacePosition.z);

	const float2 step = 1.0f / screenResolution;
    const float2 range = 10.5f * step;
    float samples = 0.0f;
    float visibility = 0.0f;
    for (float y = -range.y; y <= range.y; y += step.y)
    {
        for (float x = -range.x; x <= range.x; x += step.x)
        {
            samples += 1.0f;
            visibility += depthTx.SampleLevel(PointClampSampler, PassCB2.sunScreenSpacePosition.xy + float2(x, y), 0).r >= referenceDepth - 0.001 ? 1 : 0;
        }
    }
    visibility /= samples;
    
    p1.Pos = float4(0, 0, 0, 1);
    p1.Opacity = float4(visibility, 0, 0, 0);
    [branch]
    if (visibility > 0)
    {
        Append(triStream, p1, p[0].VertexId, mods[p[0].VertexId], flareSize);
    }
}

float4 LensFlarePS(GeometryOut In) : SV_TARGET
{
    Texture2D lensTx0 = ResourceDescriptorHeap[PassCB.lensIdx0];
    Texture2D lensTx1 = ResourceDescriptorHeap[PassCB.lensIdx1];
    Texture2D lensTx2 = ResourceDescriptorHeap[PassCB.lensIdx2];
    Texture2D lensTx3 = ResourceDescriptorHeap[PassCB.lensIdx3];
    Texture2D lensTx4 = ResourceDescriptorHeap[PassCB.lensIdx4];
    Texture2D lensTx5 = ResourceDescriptorHeap[PassCB.lensIdx5];
    Texture2D lensTx6 = ResourceDescriptorHeap[PassCB.lensIdx6];               
    Texture2D depthTx = ResourceDescriptorHeap[PassCB.depthIdx];
    
    float4 color = 0.0f;
	[branch]
    switch (In.Selector)
    {
        case 0:
            //color = lensTx0.SampleLevel(PointClampSampler, In.TexPos.xy, 0);
            color = float4(0.0f, 0.0f, 0.0f, 1.0f);
            break;                                         
        case 1:                                            
            color = lensTx1.SampleLevel(PointClampSampler, In.TexPos.xy, 0);
            break;                                         
        case 2:                                            
            color = lensTx2.SampleLevel(PointClampSampler, In.TexPos.xy, 0);
            break;                                         
        case 3:                                            
            color = lensTx3.SampleLevel(PointClampSampler, In.TexPos.xy, 0);
            break;                                         
        case 4:                                            
            color = lensTx4.SampleLevel(PointClampSampler, In.TexPos.xy, 0);
            break;                                         
        case 5:                                            
            color = lensTx5.SampleLevel(PointClampSampler, In.TexPos.xy, 0);
            break;                                         
        case 6:                                            
            color = lensTx6.SampleLevel(PointClampSampler, In.TexPos.xy, 0);
            break;
        default:
            break;
    };

    color *= 1.1 - saturate(In.TexPos.z);
    color *= In.Opacity.x;
    return color;
}
