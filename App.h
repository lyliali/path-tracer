/**
  \file App.h

  The G3D 10.00 default starter app is configured for OpenGL 4.1 and
  relatively recent GPUs.
 */
#pragma once
#include <G3D/G3DAll.h>
#include "PathTracer.h"

/** \brief Application framework. */
class App : public GApp {
protected:
    
    shared_ptr<Texture> m_result;

    int m_scatteringEvents = 10;
    int m_transportPaths = 10;
    const Array<String> m_resolutions = Array<String>("1x1", "320x200", "640x400", "1280x720");
    int m_resolutionsIndex = 1;


    /** Called from onInit */
    void makeGUI();

public:
    
    App(const GApp::Settings& settings = GApp::Settings());
    virtual void onInit() override;
    virtual void onAfterLoadScene(const Any& any, const String& sceneName) override;

};
