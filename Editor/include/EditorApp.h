#pragma once
#include <LevyeForge.h>
#include "EditorLayer.h"

#define now_width 1800
#define now_height 1000

using namespace LevyeForge;
class EditorApp : public Application {
public:
    EditorApp(ApplicationCommandLineArgs args)
        :Application("Levye Forge Editor",{now_width, now_height}, true, args){
            PushLayer(new EditorLayer({now_width, now_height}));
        }

    ~EditorApp(){}
};