#include "SandboxApp.h"
#include <Core/EntryPoint.h>
#include <LevyeForge.h>

LevyeForge::Application *LevyeForge::CreateApplication(LevyeForge::ApplicationCommandLineArgs args) {
  return new SandboxApp(args);
}