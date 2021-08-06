
typedef BuiltInTriangleIntersectionAttributes HitAttributes;

//payloads
struct ShadowPayload
{
    float Visibility;
};

//ray types
static const int ShadowRay = 0;



//util functions
float3 HitWorldPosition()
{
    return WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
}