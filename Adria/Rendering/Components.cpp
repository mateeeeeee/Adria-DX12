#include "Components.h"
#include "Graphics/GfxCommandList.h"

namespace adria
{
	void Draw(SubMesh const& submesh, GfxCommandList* cmd_list, bool override_topology /*= false*/, GfxPrimitiveTopology new_topology /*= GfxPrimitiveTopology::Undefined*/)
	{
		cmd_list->SetTopology(override_topology ? new_topology : submesh.topology);
		GfxVertexBufferView vbvs[] = { GfxVertexBufferView(submesh.vertex_buffer.get()) };
		cmd_list->SetVertexBuffers(vbvs);
		if (submesh.index_buffer)
		{
			GfxIndexBufferView ibv(submesh.index_buffer.get());
			cmd_list->SetIndexBuffer(&ibv);
			cmd_list->DrawIndexed(submesh.indices_count, submesh.instance_count, submesh.start_index_location,
				submesh.base_vertex_location, submesh.start_instance_location);
		}
		else cmd_list->Draw(submesh.vertex_count, submesh.instance_count, submesh.start_vertex_location, submesh.start_instance_location);
	}
}

