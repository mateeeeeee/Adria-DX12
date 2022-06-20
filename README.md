# Adria-DX12

Graphics engine written in C++ using DirectX12/DXR. For successful build you will need textures that you can find [here](https://github.com/mateeeeeee/Adria-DX11/releases/tag/1.0).
## Features
* Render Graph
	- Automatic Resource Barriers
	- Resource Reuse using Resource Pool
	- Automatic Resource Bind Flags and Initial State Deduction
	- Pass Culling
	- Optional Multithreaded Command List Building
* Tiled Deferred Rendering 
* Clustered Deferred Rendering
* Deferred + Forward Rendering 
* Physically Based Shading
* Image Based Lighting
* Normal Mapping
* Ray Traced Shadows (DXR)
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
* Ambient Occlusion: SSAO, HBAO, RTAO (DXR)
* Reflections: SSR, RTR (DXR)
* SSCS
* Deferred Decals
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
* Particles
* ImGui Editor
* Profiler
* Model Loading with tinygltf
* Shader Hot Reload
* Entity-Component System
* Camera and Light Frustum Culling
* Bindless Texturing 
* DirectX12 Render Passes

## TODO
* ComputeAsync using RenderGraph
* Improve DXR features
* Add DXR Global Illumination

## Dependencies
[tinygltf](https://github.com/syoyo/tinygltf)

[ImGui](https://github.com/ocornut/imgui)

[ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)

[ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog)

[stb](https://github.com/nothings/stb)

[DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler)

[D3D12MemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator)

[FastNoiseLite](https://github.com/Auburn/FastNoiseLite)

[json](https://github.com/nlohmann/json)

## Screenshots

Since this is DirectX 12 port of this [engine](https://github.com/mate286/Adria-DX11), you can see some of the screenshots there. 
All of DXR screenshots will go here:

<table>
  <tr>
    <td>Cascaded Shadow Maps</td>
     <td>Hard Ray Traced Shadows</td>
     </tr>
  <tr>
    <td><img src="Screenshots/cascades.png"></td>
    <td><img src="Screenshots/rtshadows.png"></td>
  </tr>
</table>

Ray Traced Reflections, RTAO and Depth of Field Bokeh
![alt text](Screenshots/rtr.png "Ray Traced Reflections") 
