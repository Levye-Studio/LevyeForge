#pragma once

#include "Core/Config.h"

namespace LevyeForge {

    struct BoneInfo{
        /*id is index in finalBoneMatrices*/
        int id;
        int paletteIndex = -1;

        /*offset matrix transforms vertex from model space to bone space*/
        glm::mat4 offset;
    };
}