#pragma once
#include "Application.h"
#include <PingPong.h>
#include <LevyeForge.h>

class SandboxApp : public Application {
public:
  SandboxApp(ApplicationCommandLineArgs args)
      : Application("SandBox", {now_width, now_height}, false, args) {
    PushLayer(new PingPong());
  }
};