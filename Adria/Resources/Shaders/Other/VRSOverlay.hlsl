#include "CommonResources.hlsli"

//https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/blob/main/samples/vrs/shaders/VrsOverlay.hlsl

static const uint FFX_VARIABLESHADING_RATE1D_1X = 0x0;
static const uint FFX_VARIABLESHADING_RATE1D_2X = 0x1;
static const uint FFX_VARIABLESHADING_RATE1D_4X = 0x2;

#define FFX_VARIABLESHADING_MAKE_SHADING_RATE(x,y) ((x << 2) | (y))
static const uint FFX_VARIABLESHADING_RATE_1X1 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_1X, FFX_VARIABLESHADING_RATE1D_1X); 
static const uint FFX_VARIABLESHADING_RATE_1X2 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_1X, FFX_VARIABLESHADING_RATE1D_2X); 
static const uint FFX_VARIABLESHADING_RATE_2X1 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_2X, FFX_VARIABLESHADING_RATE1D_1X); 
static const uint FFX_VARIABLESHADING_RATE_2X2 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_2X, FFX_VARIABLESHADING_RATE1D_2X); 
static const uint FFX_VARIABLESHADING_RATE_2X4 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_2X, FFX_VARIABLESHADING_RATE1D_4X); 
static const uint FFX_VARIABLESHADING_RATE_4X2 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_4X, FFX_VARIABLESHADING_RATE1D_2X); 
static const uint FFX_VARIABLESHADING_RATE_4X4 = FFX_VARIABLESHADING_MAKE_SHADING_RATE(FFX_VARIABLESHADING_RATE1D_4X, FFX_VARIABLESHADING_RATE1D_4X); 

struct VSToPS
{
	float4 Pos : SV_POSITION;
	float2 Tex : TEX;
};

struct VRSOverlayConstants
{
	uint vrsImageIdx;
    uint vrsTileSize;
};
ConstantBuffer<VRSOverlayConstants> VRSOverlayPassCB : register(b1);

float4 VRSOverlayPS(VSToPS input) : SV_Target0
{
	Texture2D<uint> vrsImageTexture = ResourceDescriptorHeap[VRSOverlayPassCB.vrsImageIdx];
	uint tileSize = VRSOverlayPassCB.vrsTileSize;

    int2 pos = input.Pos.xy / tileSize;
    float3 color = 1.0f;
	switch (vrsImageTexture[pos].r)
    {
    case FFX_VARIABLESHADING_RATE_1X1:
        color = float3(0.5, 0.0, 0.0);
        break;
    case FFX_VARIABLESHADING_RATE_1X2:
        color = float3(0.5, 0.5, 0.0);
        break;
    case FFX_VARIABLESHADING_RATE_2X1:
        color = float3(0.5, 0.25, 0.0);
        break;
    case FFX_VARIABLESHADING_RATE_2X2:
        color = float3(0.0, 0.5, 0.0);
        break;
    case FFX_VARIABLESHADING_RATE_2X4:
        color = float3(0.25, 0.25, 0.5);
        break;
    case FFX_VARIABLESHADING_RATE_4X2:
        color = float3(0.5, 0.25, 0.5);
        break;
    case FFX_VARIABLESHADING_RATE_4X4:
        color = float3(0.0, 0.5, 0.5);
        break;
    }

    int2 grid = int2(input.Pos.xy) % tileSize;
    bool border = (grid.x == 0) || (grid.y == 0);
    return float4(color, 0.5) * (border ? 0.5f : 1.0f);
}