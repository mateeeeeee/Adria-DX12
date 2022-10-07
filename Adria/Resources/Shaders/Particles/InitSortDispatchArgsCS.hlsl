#include "../Util/RootSignatures.hlsli"

RWBuffer<uint> DispatchArgs : register(u0);

cbuffer NumElementsCB : register(b11)
{
    int4 NumElements;
};


[RootSignature(InitSortDispatchArgs_RS)]
[numthreads(1, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
    DispatchArgs[0] = ((NumElements.x - 1) >> 9) + 1;
    DispatchArgs[1] = 1;
    DispatchArgs[2] = 1;
    DispatchArgs[3] = 0;
}