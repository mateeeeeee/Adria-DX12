#pragma once
#include <optional>
#include "GfxDescriptor.h"
#include "GfxResourceCommon.h"

namespace adria
{
    enum class GfxLoadAccessOp : uint8
    {
        Discard,
        Preserve,
        Clear,
        NoAccess
    };

	enum class GfxStoreAccessOp : uint8
	{
		Discard,
		Preserve,
		Resolve,
		NoAccess
	};

    enum GfxRenderPassFlagBit : uint32
    {
        GfxRenderPassFlagBit_None = 0x0,
        GfxRenderPassFlagBit_ReadOnlyDepth = 0x1,
        GfxRenderPassFlagBit_ReadOnlyStencil = 0x2,
        GfxRenderPassFlagBit_AllowUAVWrites = 0x4,
        GfxRenderPassFlagBit_SuspendingPass = 0x8,
        GfxRenderPassFlagBit_ResumingPass = 0x10,
    };
    ENABLE_ENUM_BIT_OPERATORS(GfxRenderPassFlagBit);
    using GfxRenderPassFlags = uint32;

    struct GfxColorAttachmentDesc
    {
        GfxDescriptor cpu_handle;
        GfxLoadAccessOp beginning_access;
        GfxStoreAccessOp ending_access;
        GfxClearValue clear_value;
    };

    struct GfxDepthAttachmentDesc
    {
        GfxDescriptor       cpu_handle;
        GfxLoadAccessOp     depth_beginning_access;
        GfxStoreAccessOp    depth_ending_access;
        GfxLoadAccessOp     stencil_beginning_access = GfxLoadAccessOp::NoAccess;
        GfxStoreAccessOp    stencil_ending_access = GfxStoreAccessOp::NoAccess;
        GfxClearValue       clear_value;
    };

    struct GfxRenderPassDesc
    {
        std::vector<GfxColorAttachmentDesc> rtv_attachments{};
        std::optional<GfxDepthAttachmentDesc> dsv_attachment = std::nullopt;
        GfxRenderPassFlags flags = GfxRenderPassFlagBit_None;
        uint32 width = 0;
        uint32 height = 0;
        bool legacy = false;
    };
}