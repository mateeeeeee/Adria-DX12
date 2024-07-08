#include "CommonResources.hlsli"

struct VSToGS
{
    float4 Pos : SV_POSITION;
    nointerpolation uint VertexId : VERTEXID;
};

struct GSToPS
{
    float4 Pos : SV_POSITION;
    float3 TexPos : TEXCOORD0; 
    nointerpolation uint Selector : TEXCOORD1; 
    nointerpolation float4 Opacity : TEXCOORD2;
};

static const float mods[] = { 1, 0.55, 0.4, 0.1, -0.1, -0.3, -0.5 };

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

ConstantBuffer<LensFlareConstants> LensFlarePassCB : register(b1);
ConstantBuffer<LensFlareConstants2> LensFlareLensFlarePassCB2 : register(b2);


void Append(inout TriangleStream<GSToPS> triStream, GSToPS p1, uint selector, float2 posMod, float2 size)
{
    float2 pos = (LensFlareLensFlarePassCB2.sunScreenSpacePosition.xy - 0.5f) * float2(2.0f, -2.0f);
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

VSToGS LensFlareVS(uint VertexID : SV_VERTEXID)
{
    VSToGS o = (VSToGS)0;
    o.Pos = 0;
    o.VertexId = VertexID;
    return o;
}

[maxvertexcount(4)]
void LensFlareGS(point VSToGS p[1], inout TriangleStream<GSToPS> triStream)
{
    Texture2D lensTexture0 = ResourceDescriptorHeap[LensFlarePassCB.lensIdx0];
    Texture2D lensTexture1 = ResourceDescriptorHeap[LensFlarePassCB.lensIdx1];
    Texture2D lensTexture2 = ResourceDescriptorHeap[LensFlarePassCB.lensIdx2];
    Texture2D lensTexture3 = ResourceDescriptorHeap[LensFlarePassCB.lensIdx3];
    Texture2D lensTexture4 = ResourceDescriptorHeap[LensFlarePassCB.lensIdx4];
    Texture2D lensTexture5 = ResourceDescriptorHeap[LensFlarePassCB.lensIdx5];
    Texture2D lensTexture6 = ResourceDescriptorHeap[LensFlarePassCB.lensIdx6];
    Texture2D depthTexture = ResourceDescriptorHeap[LensFlarePassCB.depthIdx];
    
    GSToPS p1 = (GSToPS) 0;
    float2 flareSize = float2(256, 256);
	[branch]
    switch (p[0].VertexId)
    {
        case 0:
            lensTexture0.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 1:
            lensTexture1.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 2:
            lensTexture2.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 3:
            lensTexture3.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 4:
            lensTexture4.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 5:
            lensTexture5.GetDimensions(flareSize.x, flareSize.y);
            break;
        case 6:
            lensTexture6.GetDimensions(flareSize.x, flareSize.y);
            break;
        default:
            break;
    };

    float2 screenResolution = FrameCB.renderResolution;
    flareSize /= screenResolution;
    float referenceDepth = saturate(LensFlareLensFlarePassCB2.sunScreenSpacePosition.z);

	const float2 step = 1.0f / screenResolution;
    const float2 range = 10.5f * step;
    float samples = 0.0f;
    float visibility = 0.0f;
    for (float y = -range.y; y <= range.y; y += step.y)
    {
        for (float x = -range.x; x <= range.x; x += step.x)
        {
            samples += 1.0f;
            visibility += depthTexture.SampleLevel(PointClampSampler, LensFlareLensFlarePassCB2.sunScreenSpacePosition.xy + float2(x, y), 0).r <= referenceDepth + 0.001 ? 1 : 0;
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

float4 LensFlarePS(GSToPS In) : SV_TARGET
{
    Texture2D lensTexture0 = ResourceDescriptorHeap[LensFlarePassCB.lensIdx0];
    Texture2D lensTexture1 = ResourceDescriptorHeap[LensFlarePassCB.lensIdx1];
    Texture2D lensTexture2 = ResourceDescriptorHeap[LensFlarePassCB.lensIdx2];
    Texture2D lensTexture3 = ResourceDescriptorHeap[LensFlarePassCB.lensIdx3];
    Texture2D lensTexture4 = ResourceDescriptorHeap[LensFlarePassCB.lensIdx4];
    Texture2D lensTexture5 = ResourceDescriptorHeap[LensFlarePassCB.lensIdx5];
    Texture2D lensTexture6 = ResourceDescriptorHeap[LensFlarePassCB.lensIdx6];               
    Texture2D depthTexture = ResourceDescriptorHeap[LensFlarePassCB.depthIdx];
    
    float4 color = 0.0f;
	[branch]
    switch (In.Selector)
    {
        case 0:
            //color = lensTexture0.SampleLevel(PointClampSampler, In.TexPos.xy, 0);
            color = float4(0.0f, 0.0f, 0.0f, 1.0f);
            break;                                         
        case 1:                                            
            color = lensTexture1.SampleLevel(PointClampSampler, In.TexPos.xy, 0);
            break;                                         
        case 2:                                            
            color = lensTexture2.SampleLevel(PointClampSampler, In.TexPos.xy, 0);
            break;                                         
        case 3:                                            
            color = lensTexture3.SampleLevel(PointClampSampler, In.TexPos.xy, 0);
            break;                                         
        case 4:                                            
            color = lensTexture4.SampleLevel(PointClampSampler, In.TexPos.xy, 0);
            break;                                         
        case 5:                                            
            color = lensTexture5.SampleLevel(PointClampSampler, In.TexPos.xy, 0);
            break;                                         
        case 6:                                            
            color = lensTexture6.SampleLevel(PointClampSampler, In.TexPos.xy, 0);
            break;
        default:
            break;
    };

    color *= 1.1 - saturate(In.TexPos.z);
    color *= In.Opacity.x;
    return color;
}
