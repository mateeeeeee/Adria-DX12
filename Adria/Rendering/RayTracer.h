#pragma once


namespace adria
{



	class RayTracer
	{
	public:
		RayTracer() {}

		void OnResize() {}
		void SceneInitialize() {}

		void Update();

		void AddRayTracedShadowsPass();
		void AddRayTracedReflectionsPass();
		void AddRayTracedAmbientOcclusionPass();

	private:
	};
}