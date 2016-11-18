/** \file PathTracer.cpp */
#include "PathTracer.h"
#include <G3D/G3DAll.h>

PathTracer::PathTracer(const Options& options) {
    m_options = options;
};

Radiance3 PathTracer::measureLight(const Ray& ray, const Ray& shadowRay, const shared_ptr<Surfel>& surfel,
    const Radiance3 biradiance, const TriTree& triTree) const {
    
    Radiance3 L_o = Radiance3();

    Vector3 wi = -shadowRay.direction();
    const Vector3 wo = -ray.direction();
    const Color3 bsdf = surfel->finiteScatteringDensity(wi, wo);
    float angleCompensation = abs( surfel->shadingNormal.dot(wi) );

    L_o += biradiance * bsdf * angleCompensation;

    return L_o;
};


shared_ptr<Light> PathTracer::getLight(const Array<shared_ptr<Light>>& lightSources, const Array<shared_ptr<Surfel>>& surfelBuffer, Array<Biradiance3>& biradianceBuffer, int index) const {
    shared_ptr<Light> light;
    Radiance3 radiance;
    shared_ptr<Surfel> surfel = surfelBuffer[index];

    if ( lightSources.size() == 1 ) {
        light = lightSources[0];
        if ( notNull(surfel) ) {
            radiance = light->biradiance(surfel->position);
        }
        biradianceBuffer[index] = radiance;
        return light;
    }
    
    // if there is more than one light source, pick a light based on importance sampling
    if ( notNull(surfel) ) {
        float totalBiradiance = 0.0f;
        for (int i = 0; i < lightSources.size(); ++i) {
            totalBiradiance += lightSources[i]->biradiance(surfel->position.xyz()).sum();
        }

        float randomNumber = Random::threadCommon().uniform(0.0f, totalBiradiance);

        int randomIndex = 0;
        float counter = randomNumber;
        light = lightSources[0];
        while (counter > 0) {
            counter -= lightSources[randomIndex]->biradiance(surfel->position.xyz()).sum();
            light = lightSources[randomIndex];
            randomIndex++;
        }

        float probability = 0.0f;
        if ( totalBiradiance > 0.0f ) {
            probability = light->biradiance(surfel->position.xyz()).sum() / totalBiradiance;
        }
        radiance = light->biradiance(surfel->position.xyz());
        Color3 biradiance = Color3::black();

        if ( probability > 0 ) {
            biradiance = radiance / probability;
        }

        biradianceBuffer[index] = biradiance;
        return light;
    }

    // surfel was null, that means the ray associated with this surfel came to the camera from the sky
    biradianceBuffer[index] = Biradiance3::black();
    return nullptr;
}


Ray& PathTracer::computeShadowRay(const Array<shared_ptr<Surfel>>& surfelBuffer, const shared_ptr<Light>& light, const int index) const {
     Ray ray;
     shared_ptr<Surfel> surfel = surfelBuffer[index];
     if( notNull(surfel) ){
         const Vector3 fromLightToSurf = (surfel->position) - light->position().xyz();
         ray = Ray(light->position().xyz(), fromLightToSurf.direction(), 0.0f, fromLightToSurf.length() - 0.001f);
     }
     return ray;
}


Color3 PathTracer::computePixelColor(const Array<shared_ptr<Surfel>>& surfelBuffer, const Array<bool>& lightShadowedBuffer,
	const Array<Color3>& modulationBuffer, const Array<Ray>& rayBuffer, const Array<Biradiance3>& biradianceBuffer,
	const Array<Ray>& shadowRayBuffer, const int index, const TriTree& triTree) const {

      shared_ptr<Surfel> surfel = surfelBuffer[index];
      Color3 color = Color3::black();
      if(notNull(surfel)){
          color = surfel->emittedRadiance(rayBuffer[index].direction()) * modulationBuffer[index];
          if(!lightShadowedBuffer[index]){
              color += measureLight(rayBuffer[index], shadowRayBuffer[index], surfel, biradianceBuffer[index], triTree)
				  * modulationBuffer[index];
          }
      }
      return color;
}


void PathTracer::update(Array<Ray>& rayBuffer, Array<Color3>& modulationBuffer, const Array<shared_ptr<Surfel>>& surfelBuffer, int index) const {
      Ray ray = rayBuffer[index];
      shared_ptr<Surfel> surfel = surfelBuffer[index];

      Vector3 w_o = -ray.direction();
      Color3 weight;
      Vector3 w_i;
      surfel->scatter(PathDirection::EYE_TO_SOURCE, w_o, true, Random::threadCommon(), weight, w_i);

      rayBuffer[index] = Ray(surfel->position + surfel->geometricNormal * 0.001f * sign(w_i.dot(surfel->geometricNormal)), w_i);
      modulationBuffer[index] *= weight;
}


/** Contains main loop that calculates total light for each pixel in the image, constructs the image, and then processes it */
void PathTracer::buildImage(const shared_ptr<Image>& image, const shared_ptr<Camera>& camera, const shared_ptr<Scene>& scene, Stopwatch* sw) const {

    const int width = image->width();
    const int height = image->height();

    Array<shared_ptr<Light>> lightSources = scene->lightingEnvironment().lightArray;
    Array<shared_ptr<Surface>> surfaces;
    scene->onPose(surfaces);
    TriTree triTree;
    triTree.setContents(surfaces);

    float numPixels = width * height;

    Array<Color3>               modulationBuffer;
    Array<Ray>                  rayBuffer;
    Array<shared_ptr<Surfel>>   surfelBuffer;
    Array<Biradiance3>          biradianceBuffer;
    Array<Ray>                  shadowRayBuffer;
    Array<bool>                 lightShadowedBuffer;

    modulationBuffer.resize(numPixels);
    rayBuffer.resize(numPixels);
    surfelBuffer.resize(numPixels);
    biradianceBuffer.resize(numPixels);
    shadowRayBuffer.resize(numPixels);
    lightShadowedBuffer.resize(numPixels);

    sw->tick();

    for (int i = 0; i < m_options.transportPaths; ++i) {
        debugPrintf("Transport path %d out of %d\n", i, m_options.transportPaths);

        modulationBuffer.setAll( Color3(1.0 / (float)m_options.transportPaths, 1.0 / (float)m_options.transportPaths, 1.0 / (float)m_options.transportPaths) );

        // find the rays from camera to scene for each pixel and put them in the rayBuffer
        for(Point2int32 pixel; pixel.y < height; ++pixel.y) {
            for(pixel.x = 0; pixel.x < width; ++pixel.x) {
                int offsetX;
                int offsetY;

                if ( m_options.transportPaths == 1 ) {
                    offsetX = 0.5f;
                    offsetY = 0.5f;
                } else {
                    offsetX = Random::threadCommon().uniform();
                    offsetY = Random::threadCommon().uniform();
                }

                rayBuffer[pixel.y * width + pixel.x] = camera->worldRay((float)pixel.x + offsetX,
																		(float)pixel.y + offsetY,
																		Rect2D(Vector2(width, height)));
            }
        }

        for (int j = 0; j < m_options.scatteringEvents; ++j) {
            // find intersection, storing results in surfelBuffer
            triTree.intersectRays(rayBuffer, surfelBuffer);
            
            // add emissive terms (use the sky's radiance for missed rays and apply the modulation buffer)
            Thread::runConcurrently(Point2int32(0,0), Point2int32(width,height), [this, &lightSources, &surfelBuffer,
				&biradianceBuffer, &shadowRayBuffer, width](Point2int32 pixel){
                
				int index = pixel.y * width + pixel.x;
                shared_ptr<Light> light = getLight(lightSources, surfelBuffer, biradianceBuffer, index);
                
                Ray ray = computeShadowRay(surfelBuffer, light, index);
                shadowRayBuffer[index] = ray;
            });

            triTree.intersectRays(shadowRayBuffer, lightShadowedBuffer);

            Thread::runConcurrently(Point2int32(0,0), Point2int32(width,height), [this, j, &image, &surfelBuffer, &rayBuffer,
				&shadowRayBuffer, &triTree, &lightShadowedBuffer, &biradianceBuffer, &modulationBuffer, width](Point2int32 pixel){
                
				int index = pixel.y * width + pixel.x;
                Color3 light = computePixelColor(surfelBuffer, lightShadowedBuffer, modulationBuffer,
					rayBuffer, biradianceBuffer, shadowRayBuffer, index, triTree);
                image->increment(Point2int32(pixel.x, pixel.y), light);

                if(j < (m_options.scatteringEvents - 1) && notNull(surfelBuffer[index])){
                    update(rayBuffer, modulationBuffer, surfelBuffer, index);
                }
            });
        }
    }

    sw->tock();
}