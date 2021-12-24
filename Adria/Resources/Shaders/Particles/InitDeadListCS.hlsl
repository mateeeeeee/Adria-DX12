#include "../Util/RootSignatures.hlsli"

AppendStructuredBuffer<uint> deadList : register(u0);

[RootSignature(InitDeadList_RS)]
[numthreads(256, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	deadList.Append(id.x);
}