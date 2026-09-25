#include <LevyeForge.h>
#include <Core/EntryPoint.h>
#include "EditorApp.h"

LevyeForge::Application* LevyeForge::CreateApplication(LevyeForge::ApplicationCommandLineArgs args){

    return new EditorApp(args);
}