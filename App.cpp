/** \file App.cpp */
#include "App.h"
#include "PathTracer.h"
#include <cmath>
#include <iostream>
#include <sstream>

// Tells C++ to invoke command-line main() function even on OS X and Win32.
G3D_START_AT_MAIN();

int main(int argc, const char* argv[]) {
    {
        G3DSpecification g3dSpec;
        g3dSpec.audio = false;
        initGLG3D(g3dSpec);
    }

    GApp::Settings settings(argc, argv);

    // Change the window and other startup parameters by modifying the
    // settings class.  For example:
    settings.window.caption             = argv[0];

    // Set enable to catch more OpenGL errors
    // settings.window.debugContext     = true;

    // Some common resolutions:
    // settings.window.width            =  854; settings.window.height       = 480;
    // settings.window.width            = 1024; settings.window.height       = 768;
    settings.window.width               = 1280; settings.window.height       = 720;
    // settings.window.width             = 1920; settings.window.height       = 1080;
    // settings.window.width            = OSWindow::primaryDisplayWindowSize().x; settings.window.height = OSWindow::primaryDisplayWindowSize().y;
    settings.window.fullScreen          = false;
    settings.window.resizable           = ! settings.window.fullScreen;
    settings.window.framed              = ! settings.window.fullScreen;

    // Set to true for a significant performance boost if your app can't render at 60fps, or if
    // you *want* to render faster than the display.
    settings.window.asynchronous        = false;

    settings.hdrFramebuffer.depthGuardBandThickness = Vector2int16(0, 0);
    settings.hdrFramebuffer.colorGuardBandThickness = Vector2int16(0, 0);
    settings.dataDir                    = FileSystem::currentDirectory();
    settings.screenshotDirectory        = "../journal/";

    settings.renderer.deferredShading = true;
    settings.renderer.orderIndependentTransparency = false;

    return App(settings).run();
}


App::App(const GApp::Settings& settings) : GApp(settings) {
}


// Called before the application loop begins.  Load data here and
// not in the constructor so that common exceptions will be
// automatically caught.
void App::onInit() {
    GApp::onInit();
    setFrameDuration(1.0f / 120.0f);

    // Call setScene(shared_ptr<Scene>()) or setScene(MyScene::create()) to replace
    // the default scene here.
    
    showRenderingStats      = false;

    makeGUI();
    // For higher-quality screenshots:
    // developerWindow->videoRecordDialog->setScreenShotFormat("PNG");
    // developerWindow->videoRecordDialog->setCaptureGui(false);
    developerWindow->cameraControlWindow->moveTo(Point2(developerWindow->cameraControlWindow->rect().x0(), 0));
    loadScene(
        "G3D Cornell Box" // Load something simple
        //developerWindow->sceneEditorWindow->selectedSceneName()  // Load the first scene encountered 
        );
}


void App::onAfterLoadScene(const Any& any, const String& sceneName) {
    GApp::onAfterLoadScene(any, sceneName);

    Array<shared_ptr<Camera>> cameras;
    scene()->getTypedEntityArray(cameras);

    for (int i = 0; i < cameras.size(); ++i) {
        FilmSettings settings = cameras[i]->filmSettings();
        settings.setBloomStrength(0.0f);
        settings.setAntialiasingEnabled(false);
        settings.setVignetteBottomStrength(0.0f);
        settings.setVignetteTopStrength(0.0f);
        settings.setVignetteSizeFraction(0.0f);
        cameras[i]->motionBlurSettings().setEnabled(false);
        cameras[i]->depthOfFieldSettings().setEnabled(false);
    }

    FilmSettings settings = m_debugCamera->filmSettings();
    settings.setBloomStrength(0.0f);
    settings.setAntialiasingEnabled(false);
    settings.setVignetteBottomStrength(0.0f);
    settings.setVignetteTopStrength(0.0f);
    settings.setVignetteSizeFraction(0.0f);
    m_debugCamera->motionBlurSettings().setEnabled(false);
    m_debugCamera->depthOfFieldSettings().setEnabled(false);
}


void App::makeGUI() {
    // Initialize the developer HUD
    createDeveloperHUD();

    debugWindow->setVisible(true);
    developerWindow->videoRecordDialog->setEnabled(true);

	// Code to add custom renderer GUI.
    GuiPane* rendererPane = debugPane->addPane("Path Tracer");

	rendererPane->setNewChildSize(500, -1, 300);
    rendererPane->addDropDownList("Resolution", m_resolutions, &m_resolutionsIndex);
    rendererPane->addNumberBox("Number of scattering events:", &m_scatteringEvents, "", GuiTheme::LINEAR_SLIDER, 1, 10000);
    rendererPane->addNumberBox("Light transport paths:", &m_transportPaths, "per pixel", GuiTheme::LINEAR_SLIDER, 1, 2048);
    rendererPane->addButton("Render", [this](){
      drawMessage("Rendering...");
      //call render method
      PathTracer::Options options;
      options.scatteringEvents = m_scatteringEvents;
      options.transportPaths = m_transportPaths;
      PathTracer pathTracer = PathTracer(options);

      int x;
      int y;
      if (m_resolutionsIndex == 0) {
        x = 1;
        y = 1;
      }
      if (m_resolutionsIndex == 1) {
        x = 320;
        y = 200;
      }
      if (m_resolutionsIndex == 2) {
        x = 640;
        y = 400;
      }
      if (m_resolutionsIndex == 3) {
        x = 1280;
        y = 720;
      }

      shared_ptr<Image> image;
      const ImageFormat* format = ImageFormat::RGB32F();

      image = Image::create(x, y, format);
      image->setAll(Color3(0,0,0));
      Stopwatch sw = Stopwatch();
      pathTracer.buildImage(image, activeCamera(), scene(), &sw);

      // post process code from cpuRealTimeRayTrace project
      const shared_ptr<Texture>& src = Texture::fromImage("Render", image);

      FilmSettings settings = activeCamera()->filmSettings();
      settings.setBloomStrength(0.0f);
      settings.setAntialiasingEnabled(false);
      settings.setVignetteBottomStrength(0.0f);
      settings.setVignetteTopStrength(0.0f);
      settings.setVignetteSizeFraction(0.0f);
      activeCamera()->motionBlurSettings().setEnabled(false);
      activeCamera()->depthOfFieldSettings().setEnabled(false);

      //show(image, "");
      m_film->exposeAndRender(renderDevice, settings, src, 0, 0, m_result);
      
      std::ostringstream strs;
      strs << "Time elapsed: " << sw.smoothElapsedTime() << " seconds";
      show(m_result, (String) strs.str());
      shared_ptr<Image> saveImage = m_result->toImage(ImageFormat::RGB32F());
      saveImage->convert(ImageFormat::RGB8());
      saveImage->save("result.png");
    } );

    debugWindow->pack();
    debugWindow->setRect(Rect2D::xywh(0, 0,  debugWindow->rect().width(), debugWindow->rect().height()));
}