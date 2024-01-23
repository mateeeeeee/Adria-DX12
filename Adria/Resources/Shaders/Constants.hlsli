#ifndef _CONSTANTS_
#define _CONSTANTS_

static const float M_PI = 3.141592654f;
static const float M_PI_2 = 6.283185307f;
static const float M_PI_DIV_2 = 1.570796327f;
static const float M_PI_DIV_4 = 0.7853981635f;
static const float M_PI_INV = 0.318309886f;
static const float FLT_MAX = 3.402823466E+38F;
static const float FLT_EPSILON = 1.19209290E-07F;

float DegreesToRadians(float degrees)
{
    return degrees * (M_PI / 180.0f);
}

#endif