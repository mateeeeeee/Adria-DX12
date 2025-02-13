#pragma once
#ifndef _LIGHTINFO_
#define _LIGHTINFO_

#define DIRECTIONAL_LIGHT 0
#define POINT_LIGHT 1
#define SPOT_LIGHT 2

struct Light
{
	float4	position;
	float4	direction;
	float4	color;
	
	int		active;
	float	range;
	int		type;
	float	outerCosine;
	
	float	innerCosine;
	int     volumetric;
	float   volumetricStrength;
	int     useCascades;
	
	int     shadowTextureIndex;
	int     shadowMatrixIndex;
    int     shadowMaskIndex;
    int     padd;
};

#endif