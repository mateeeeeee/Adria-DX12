#include "../Globals/GlobalsCS.hlsli"

struct PickingData
{
    float4 position;
    float4 normal;
};

Texture2D<float> depthTx   : register(t0);
Texture2D<float4> normalTx : register(t1);
RWStructuredBuffer<PickingData> PickingBuffer : register(u0);

#include "../Util/RootSignatures.hlsli"
[RootSignature(Picker_RS)]
[numthreads(1, 1, 1)]
void main( )
{
    if (any(frame_cbuf.mouse_normalized_coords > 1.0f) || any(frame_cbuf.mouse_normalized_coords < 0.0f))
        return;

    uint2 mouse_coords = uint2(frame_cbuf.mouse_normalized_coords * frame_cbuf.screen_resolution);

    float zw = depthTx[mouse_coords].xx;
    float2 uv = (mouse_coords + 0.5f) / frame_cbuf.screen_resolution;
    uv = uv * 2.0f - 1.0f;
    uv.y *= -1.0f;
    float4 positionWS = mul(float4(uv, zw, 1.0f), frame_cbuf.inverse_view_projection);
    float3 normalVS = normalTx[mouse_coords].xyz;
    normalVS = 2.0 * normalVS - 1.0;

    PickingData picking_data;
    picking_data.position = positionWS / positionWS.w;
    picking_data.normal = float4(normalize(mul(normalVS, (float3x3) transpose(frame_cbuf.view))), 0.0f);
    PickingBuffer[0] = picking_data;
}