
struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 Tex : TEX;
   
};

VertexOut vs_main(uint vI : SV_VERTEXID)
{
    int2 texcoord = int2(vI & 1, vI >> 1); 
    
    VertexOut vout;
    vout.Tex = float2(texcoord);
    vout.PosH = float4(2 * (texcoord.x - 0.5f), -2 * (texcoord.y - 0.5f), 0, 1);

    return vout;
}