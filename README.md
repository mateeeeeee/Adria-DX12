# Adria-DX12

Graphics engine written in C++/DirectX12. 
## Features
* Entity-Component System
* Deferred + Forward Rendering 
* Tiled Deferred Rendering 
* Clustered Deferred Rendering
* Physically Based Shading
* Image Based Lighting
* Normal Mapping
* Shadows
    - PCF Shadows for Directional, Spot and Point lights
    - Cascade Shadow Maps for Directional Lights
* Volumetric Lighting
    - Directional Lights with Shadow Maps
    - Directional Lights with Cascade Shadow Maps
    - Point and Spot Lights 
* HDR and Tone Mapping
* Bloom
* Depth Of Field
* Bokeh
* Ambient Occlusion: SSAO, HBAO
* SSR
* FXAA
* TAA
* God Rays
* Lens Flare
* Motion Blur
* Fog
* Volumetric Clouds
* Hosek-Wilkie Sky
* Ocean FFT
    - Adaptive Tesselation
    - Foam
* ImGui Editor
* Profiler
* Model Loading with tinygltf
* Camera and Light Frustum Culling
* Multithreaded Command List Building
* DirectX 12 Render Passes


## Dependencies
[tinygltf](https://github.com/syoyo/tinygltf)

[ImGui](https://github.com/ocornut/imgui)

[ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)

[ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog)

[stb](https://github.com/nothings/stb)

[DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler)

[D3D12MemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator)

[FastNoiseLite](https://github.com/Auburn/FastNoiseLite)

## Screenshots

Since this is DirectX 12 port of this [engine](https://github.com/mate286/Adria-DX11)
(not complete though since Voxel GI and Terrain are missing), you can see some of the screenshots there.
