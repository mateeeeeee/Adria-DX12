# Adria-DX12

Graphics engine written in C++ using DirectX12/DXR. For successful build you will need textures that you can find [here](https://github.com/mateeeeeee/Adria-DX11/releases/tag/1.0).
## Features
* Render graph
	- Automatic resource barriers
	- Resource reuse using resource pool
	- Automatic resource bind flags and initial state deduction
	- Pass culling
    - Graph visualization
* Reference path tracer 
* GPU-Driven Rendering
    - GPU frustum culling
    - 2 phase GPU occlusion culling
* Ultimate Bindless 
    - Only one root signature 
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
* Depth of field + Bokeh
* Ambient occlusion: SSAO, HBAO, RTAO (DXR)
* Reflections: SSR, RTR (DXR)
* Deferred decals
* FXAA
* TAA
* God rays
* Lens flare
* Motion blur
* Fog
* Volumetric clouds
    - Temporal reprojection
* Hosek-Wilkie sky
* Ocean FFT
    - Adaptive tesselation
    - Foam
* Shader hot reload
* ImGui Editor
* Profiler
    - custom and tracy profiler
* Model Loading with tinygltf

## TODO
* DDGI (wip)
* Debug Renderer
* FSR2

## Dependencies
[ImGui](https://github.com/ocornut/imgui)

[DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler)

[D3D12MemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator)

[nativefiledialog](https://github.com/mlabbe/nativefiledialog)

[tracy](https://github.com/wolfpld/tracy)

[stb](https://github.com/nothings/stb)

[cereal](https://github.com/USCiLab/cereal)

[tinygltf](https://github.com/syoyo/tinygltf)

[json](https://github.com/nlohmann/json)

[ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo)

## Screenshots

Since this engine shares some of the features with [engine](https://github.com/mate286/Adria-DX11), you can also see some of the screenshots there. 

Volumetric Clouds
![alt text](Screenshots/clouds.png "Volumetric clouds") 

San Miguel
![alt text](Screenshots/sanmiguel.png "San Miguel") 

Path Tracer
![alt text](Screenshots/pathtracing1.png "Path tracing") 
![alt text](Screenshots/pathtracing2.png "Path tracing") 

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

<table>
  <tr>
    <td>Screen Space Reflections</td>
     <td>Ray Traced Reflections</td>
     </tr>
  <tr>
    <td><img src="Screenshots/ssr.png"></td>
    <td><img src="Screenshots/rtr.png"></td>
  </tr>
</table>

<table>
  <tr>
    <td>SSAO</td>
     <td>RTAO</td>
     </tr>
  <tr>
    <td><img src="Screenshots/ssao.png"></td>
    <td><img src="Screenshots/rtao.png"></td>
  </tr>
</table>

Render graph visualization
![alt text](Adria/rendergraph.svg "Render graph visualization") 
