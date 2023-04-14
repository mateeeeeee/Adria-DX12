# Adria-DX12

Graphics engine written in C++ using DirectX12/DXR. For successful build you will need textures that you can find [here](https://github.com/mateeeeeee/Adria-DX11/releases/tag/1.0).
## Features
* Render graph
	- Automatic resource barriers
	- Resource reuse using resource pool
	- Automatic resource bind flags and initial state deduction
	- Pass culling
* Reference path tracer 
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
* GPU-Driven Rendering 
* Improve DXR features
* Add DXR GI

## Dependencies
[ImGui](https://github.com/ocornut/imgui)

[DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler)

[D3D12MemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator)

[nativefiledialog](https://github.com/mlabbe/nativefiledialog)

[stb](https://github.com/nothings/stb)

[cereal](https://github.com/USCiLab/cereal)

[tinygltf](https://github.com/syoyo/tinygltf)

[json](https://github.com/nlohmann/json)

[ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)

## Screenshots

Since this engine shares some of the features with [engine](https://github.com/mate286/Adria-DX11), you can also see some of the screenshots there. 

<table>
  <tr>
    <td>Cascaded Shadow Maps</td>
     <td>Hard Ray Traced Shadows</td>
     </tr>
  <tr>
    <td><img src="Screenshots/cascades.png"></td>
    <td><img src="Screenshots/raytraced.png"></td>
  </tr>
</table>

Path Tracer
![alt text](Screenshots/pathtracing1.png "Path tracing") 
![alt text](Screenshots/pathtracing2.png "Path tracing") 
