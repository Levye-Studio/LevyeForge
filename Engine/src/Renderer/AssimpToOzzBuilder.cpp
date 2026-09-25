#include "Renderer/AssimpToOzzBuilder.h"
#include "Animation/AnimData.h"
#include "Log.h"
#include "assimp/anim.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/base/maths/transform.h>
#include <ozz/animation/runtime/skeleton_utils.h>
#include <cmath>
#include <algorithm>

namespace LevyeForge {

static void CollectBones(const aiScene *scene,
                         std::unordered_set<std::string> &bones) {
  for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
    aiMesh *mesh = scene->mMeshes[m];

    for (unsigned int b = 0; b < mesh->mNumBones; b++) {
      bones.insert(mesh->mBones[b]->mName.C_Str());
    }
  }
}

static bool NodeContainsBone(const aiNode *node,
                             const std::map<std::string, BoneInfo> &bones) {
  if (bones.count(node->mName.C_Str()))
    return true;

  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    if (NodeContainsBone(node->mChildren[i], bones))
      return true;
  }

  return false;
}

static ozz::math::Transform ConvertTransform(const aiMatrix4x4 &m) {
  aiVector3D scale;
  aiQuaternion rot;
  aiVector3D pos;

  m.Decompose(scale, rot, pos);

  ozz::math::Transform t;
  t.translation = ozz::math::Float3(pos.x, pos.y, pos.z);
  t.rotation = ozz::math::Quaternion(rot.x, rot.y, rot.z, rot.w);
  t.scale = ozz::math::Float3(scale.x, scale.y, scale.z);

  return t;
}

static float ToSeconds(double time, double ticksPerSecond) {
  return float(time / ticksPerSecond);
}

void AssimpSkeletonBuilder::BuildJoint(
    const aiNode *node, ozz::animation::offline::RawSkeleton::Joint &joint) {
  joint.name = node->mName.C_Str();
  joint.transform = ConvertTransform(node->mTransformation);

  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    // const aiNode *child = node->mChildren[i];

    // std::string name = child->mName.C_Str();

    // ozz::animation::offline::RawSkeleton::Joint childJoint;

    // BuildJoint(child, childJoint);

    // joint.children.push_back(childJoint);

    joint.children.emplace_back();
    BuildJoint(node->mChildren[i], joint.children.back());
  }
}
ozz::unique_ptr<ozz::animation::Skeleton>
AssimpSkeletonBuilder::Build(const aiScene *scene) {
  ozz::animation::offline::RawSkeleton raw;

  raw.roots.emplace_back();

  BuildJoint(scene->mRootNode, raw.roots[0]);

  if (!raw.Validate()) {
    LF_CORE_ERROR("[ASSIMP->OZZ] Raw skeleton invalid");
    return nullptr;
  }

  ozz::animation::offline::SkeletonBuilder builder;

  return builder(raw);
}

// ASSIMP ANIMATION BUILDER

int FindJoint(const ozz::animation::Skeleton &skel, const std::string &name) {

  for (int i = 0; i < skel.num_joints(); i++) {
    if (name == skel.joint_names()[i])
      return i;
  }

  return -1;
}

ozz::unique_ptr<ozz::animation::Animation>
AssimpAnimationBuilder::Build(const aiScene *scene,
                              const ozz::animation::Skeleton &skeleton,
                              int animIndex) {
  if (!scene || animIndex < 0 || animIndex >= static_cast<int>(scene->mNumAnimations))
    return nullptr;
  const aiAnimation *anim = scene->mAnimations[animIndex];
  const double ticks = anim->mTicksPerSecond > 0 ? anim->mTicksPerSecond : 25.0;
  ozz::animation::offline::RawAnimation raw;
  double lastTick = std::max(0.0, anim->mDuration);
  for (unsigned c = 0; c < anim->mNumChannels; ++c) {
    const auto *channel = anim->mChannels[c];
    if (channel->mNumPositionKeys) lastTick = std::max(lastTick, channel->mPositionKeys[channel->mNumPositionKeys - 1].mTime);
    if (channel->mNumRotationKeys) lastTick = std::max(lastTick, channel->mRotationKeys[channel->mNumRotationKeys - 1].mTime);
    if (channel->mNumScalingKeys) lastTick = std::max(lastTick, channel->mScalingKeys[channel->mNumScalingKeys - 1].mTime);
  }
  raw.duration = std::max(static_cast<float>(lastTick / ticks), 1.0f / 60.0f);
  if (!std::isfinite(raw.duration)) return nullptr;
  raw.tracks.resize(skeleton.num_joints());
  int matched = 0;
  for (unsigned c = 0; c < anim->mNumChannels; ++c) {
    const aiNodeAnim *channel = anim->mChannels[c];
    int joint = FindJoint(skeleton, channel->mNodeName.C_Str());
    // FBX pivot channels animate helper nodes, not their base bone. Aliasing
    // them overwrites the bone's rest translation with the helper's zero offset.
    if (joint < 0) continue;
    ++matched;
    auto &track = raw.tracks[joint];
    for (unsigned i = 0; i < channel->mNumPositionKeys; ++i) {
      const auto &key = channel->mPositionKeys[i];
      track.translations.push_back({static_cast<float>(key.mTime / ticks), {key.mValue.x, key.mValue.y, key.mValue.z}});
    }
    for (unsigned i = 0; i < channel->mNumRotationKeys; ++i) {
      const auto &key = channel->mRotationKeys[i];
      auto rotation = key.mValue;
      rotation.Normalize();
      track.rotations.push_back({static_cast<float>(key.mTime / ticks), {rotation.x, rotation.y, rotation.z, rotation.w}});
    }
    for (unsigned i = 0; i < channel->mNumScalingKeys; ++i) {
      const auto &key = channel->mScalingKeys[i];
      track.scales.push_back({static_cast<float>(key.mTime / ticks), {key.mValue.x, key.mValue.y, key.mValue.z}});
    }
  }
  if (matched == 0) return nullptr;
  // Ozz defaults empty tracks to identity, which destroys unanimated joint offsets.
  for (int joint = 0; joint < skeleton.num_joints(); ++joint) {
    const auto rest = ozz::animation::GetJointLocalRestPose(skeleton, joint);
    auto &track = raw.tracks[joint];
    if (track.translations.empty()) track.translations.push_back({0, rest.translation});
    if (track.rotations.empty()) track.rotations.push_back({0, rest.rotation});
    if (track.scales.empty()) track.scales.push_back({0, rest.scale});
  }

  if (!raw.Validate()) {
    LF_CORE_ERROR("[ASSIMPTOOZZ] raw animation failed!");
    return nullptr;
  }

  ozz::animation::offline::AnimationBuilder builder;

  return builder(raw);
}
} // namespace LevyeForge