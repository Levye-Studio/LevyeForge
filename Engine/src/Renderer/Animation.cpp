#include "Animation/Animation.h"
#include "AssimpToOzzBuilder.h"
#include <assimp/Importer.hpp>
#include <assimp/config.h>

namespace LevyeForge {
Animation::Animation(const std::string &path, const ozz::animation::Skeleton *skeleton,
                     int clipIndex)
    : m_Skeleton(skeleton), m_Path(path), m_ClipIndex(clipIndex) {
  if (!skeleton) { m_Error = "Load a model before loading an animation."; return; }
  Assimp::Importer importer;
  importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, true);
  const aiScene *scene = importer.ReadFile(path, 0);
  if (!scene || !scene->mRootNode || !scene->HasAnimations()) {
    m_Error = "Could not load animation: " + path;
    LF_CORE_ERROR("{} ({})", m_Error, importer.GetErrorString());
    return;
  }
  if (clipIndex < 0 || clipIndex >= static_cast<int>(scene->mNumAnimations)) {
    m_Error = "Clip index is outside the animation file's clip range.";
    return;
  }
  m_Animation = AssimpAnimationBuilder::Build(scene, *skeleton, clipIndex);
  if (!m_Animation)
    m_Error = "Animation has invalid keyframes or no tracks matching this model.";
}
}
