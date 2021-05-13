
struct BokehVSOutput
{
    float2 Position : POSITION;
    float2 Size     : SIZE;
    float3 Color    : COLOR;
    float  Depth    : DEPTH;
};

struct Bokeh
{
    float3 Position;
    float2 Size;
    float3 Color;
};

StructuredBuffer<Bokeh> BokehStack : register(t0);

BokehVSOutput main(in uint VertexID : SV_VertexID)
{
    Bokeh bPoint = BokehStack[VertexID];
    BokehVSOutput output;

	// Position the vertex in normalized device coordinate space [-1, 1]
    output.Position.xy = bPoint.Position.xy;
    output.Position.xy = output.Position.xy * 2.0f - 1.0f;
    output.Position.y *= -1.0f;

	// Scale the size based on the CoC size, and max bokeh sprite size
    output.Size = bPoint.Size; 
    
    output.Depth = bPoint.Position.z; //depth in view space

    output.Color = bPoint.Color;

    return output;
}