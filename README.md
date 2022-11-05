# Adria-DX12

Graphics engine written in C++ using DirectX12/DXR. For successful build you will need textures that you can find [here](https://github.com/mateeeeeee/Adria-DX11/releases/tag/1.0).
## Features
* Render graph
	- Automatic resource barriers
	- Resource reuse using resource pool
	- Automatic resource bind flags and initial state deduction
	- Pass culling
	- Optional multithreaded command list building
* Tiled deferred rendering 
* Clustered deferred rendering
* Physically based shading
* Ray traced shadows (DXR)
* Shadows
    - PCF shadows for directional, spot and point lights
    - Cascade shadow maps for directional lights
* Volumetric lighting for shadow casting lights
* HDR and tone mapping
* Automatic exposure
* Bloom
* Depth of field
* Bokeh
* Ambient occlusion: SSAO, HBAO, RTAO (DXR)
* Reflections: SSR, RTR (DXR)
* SSCS
* Deferred decals
* FXAA
* TAA
* God rays
* Lens flare
* Motion blur
* Fog
* Volumetric clouds
* Hosek-Wilkie sky
* Ocean FFT
    - Adaptive tesselation
    - Foam
* ImGui Editor
* Profiler
* Shader hot reload
* Ultimate Bindless 
    - Only one root signature 
* Model Loading with tinygltf

## TODO
* Improve DXR features
* Add DXR GI

## Dependencies
[tinygltf](https://github.com/syoyo/tinygltf)

[ImGui](https://github.com/ocornut/imgui)

[ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)

[nativefiledialog](https://github.com/mlabbe/nativefiledialog)

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
