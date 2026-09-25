#pragma once

#include "Application.h"

extern LevyeForge::Application* LevyeForge::CreateApplication(ApplicationCommandLineArgs args);

int main(int argc, char** argv){
    
    LevyeForge::Log::Init();

    LF_CORE_INFO("Loading Application");

    LF_PROFILE_BEGIN_SESSION("Startup", "LFProfile-Startup.json");
    auto app = LevyeForge::CreateApplication({ argc, argv });
    LF_PROFILE_END_SESSION();

    LF_PROFILE_BEGIN_SESSION("Runtime", "LFProfile-Runtime.json");
    app->Run();
    LF_PROFILE_END_SESSION();

    LF_CORE_INFO("Shuting down");

    LF_PROFILE_BEGIN_SESSION("Shutdown", "LFProfile-Shutdown.json");
    delete app;
    LF_PROFILE_END_SESSION();

    return 0;
}