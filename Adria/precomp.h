#pragma once
//std headers + win32/d3d12
#include <vector>
#include <memory>
#include <string>
#include <array>
#include <queue>
#include <mutex>
#include <thread>
#include <optional>
#include <functional>
#include <span>
#include <type_traits>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.h>

//external utility
#include <DirectXMath.h>
#include "d3dx12.h"
#include "D3D12MemAlloc.h"
#include "nfd.h"
#include "entt/entt.hpp"
#include "../ImGui/imgui.h"
#include "../ImGui/ImGuizmo.h"

//
#include "Core/CoreTypes.h"
#include "Core/Defines.h"
#include "Utilities/AutoRefCountPtr.h"
