/**
  \file PathTracer.h

  Traces paths.

  Edited from Ben Drews and Lylia Li's RayTracer.h
  Path tracing methods implemented by Diego Gonzalez and Lylia Li.
 */
#pragma once
#include <G3D/G3DAll.h>

class PathTracer {
public:
    class Options {
    public: 
        bool multithreading = false;
        int scatteringEvents = 1;
        int transportPaths = 10;
    };
protected:
    Options m_options;

    Radiance3 PathTracer::measureLight(const Ray& wo, const Ray& shadowRay, const shared_ptr<Surfel>& surfel, 
        const Radiance3 lightRadiance, const TriTree& triTree) const;

    shared_ptr<Light> PathTracer::getLight(const Array<shared_ptr<Light>>& lightSources, const Array<shared_ptr<Surfel>>& surfelBuffer, Array<Biradiance3>& biradianceBuffer, int index) const;

    Ray& PathTracer::computeShadowRay(const Array<shared_ptr<Surfel>>& surfelBuffer, const shared_ptr<Light>& light, const int index) const;

    Color3 PathTracer::computePixelColor(const Array<shared_ptr<Surfel>>& surfelBuffer, const Array<bool>& lightShadowedBuffer, const Array<Color3>& modulationBuffer,
        const Array<Ray>& rayBuffer, const Array<Biradiance3>& biradianceBuffer, const Array<Ray>& shadowRayBuffer, const int index, const TriTree& triTree) const;

    void PathTracer::update(Array<Ray>& rayBuffer, Array<Color3>& modulationBuffer, const Array<shared_ptr<Surfel>>& surfelBuffer, int index) const;

public:
    PathTracer(const Options& options);
    void buildImage(const shared_ptr<Image>& image, const shared_ptr<Camera>& camera, const shared_ptr<Scene>& scene, Stopwatch* sw) const;
};