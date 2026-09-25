#include "Renderer/Animation/AnimationSystem.h"
#include "Log.h"
#include "Runtime/Components.h"
#include "Runtime/Entity.h"
#include "Runtime/Scene.h"

#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/animation/runtime/skeleton_utils.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/maths/soa_float4x4.h>
#include <ozz/base/maths/soa_transform.h>
// #include <ozz/io/archive.h>
// #include <ozz/io/file.h>

namespace LevyeForge {

void AnimationSystem::Update(Scene *scene, Timestep ts) {
  scene->ViewEntity<Entity, AnimatorComponent>([&ts](auto entity, auto &anim) {
    if (!entity.template HasComponent<ModelComponent>()) { anim.HasPose = false; return; }
    const auto &model = entity.template GetComponent<ModelComponent>().ModelData;
    if (model != anim.ModelRef) anim.InitFromModel(model);
    if (!anim.ModelRef || !anim.Context) return;

    auto *skeleton = anim.ModelRef->GetSkeleton();
    auto *animation = anim.CurrentAnimation.Get();

    if (!animation)
      return;

    if (anim.Playing && std::isfinite((float)ts))
      anim.Time += std::max(0.0f, (float)ts) * std::max(0.0f, anim.Speed);

    float duration = animation->duration();

    if (!std::isfinite(duration) || duration <= 0) return;
    if (anim.Loop && anim.Playing)
      anim.Time = fmod(anim.Time, duration);
    else {
      anim.Time = glm::clamp(anim.Time, 0.0f, duration);
      if (!anim.Loop && anim.Time >= duration) anim.Playing = false;
    }

    float ratio = anim.Time / duration;

    // LF_CORE_WARN("Skeleton joints: {}", skeleton->num_joints());
    // LF_CORE_WARN("Animation tracks: {}", animation->num_tracks());
    // LF_CORE_WARN("Context tracks: {}", anim.Context->max_soa_tracks());

    // Sampling
    ozz::animation::SamplingJob sampling;
    sampling.animation = animation;
    sampling.context = anim.Context.get();
    sampling.ratio = ratio;
    sampling.output = ozz::make_span(anim.LocalTransforms);

    if (!sampling.Run()) {
      LF_CORE_ERROR("Sampling failed");
      return;
    }

    if (!anim.BlendFrom.empty()) {
      if (anim.Playing) anim.BlendTime += std::max(0.0f, (float)ts);
      const float alpha = glm::clamp(anim.BlendTime / anim.BlendDuration, 0.0f, 1.0f);
      ozz::animation::BlendingJob::Layer layers[2];
      layers[0].transform = ozz::make_span(anim.BlendFrom);
      layers[0].weight = 1-alpha;
      layers[1].transform = ozz::make_span(anim.LocalTransforms);
      layers[1].weight = alpha;
      ozz::animation::BlendingJob blend;
      blend.layers = ozz::make_span(layers);
      blend.rest_pose = skeleton->joint_rest_poses();
      blend.output = ozz::make_span(anim.BlendOutput);
      if (blend.Run()) anim.LocalTransforms.swap(anim.BlendOutput);
      if (alpha >= 1) anim.BlendFrom.clear();
    }
    // Optional in-place locomotion: physics owns character displacement.
    if (!anim.InPlaceJoint.empty()) {
      for (int joint=0;joint<skeleton->num_joints();++joint) {
        if (anim.InPlaceJoint != skeleton->joint_names()[joint]) continue;
        const auto rest = ozz::animation::GetJointLocalRestPose(*skeleton, joint);
        auto &translation = anim.LocalTransforms[joint/4].translation;
        translation.x = ozz::math::SetI(translation.x, ozz::math::simd_float4::Load1(rest.translation.x), joint%4);
        translation.y = ozz::math::SetI(translation.y, ozz::math::simd_float4::Load1(rest.translation.y), joint%4);
        translation.z = ozz::math::SetI(translation.z, ozz::math::simd_float4::Load1(rest.translation.z), joint%4);
      }
    }

    // Local → Model
    ozz::animation::LocalToModelJob ltm;
    ltm.skeleton = skeleton;
    ltm.input = ozz::make_span(anim.LocalTransforms);
    ltm.output = ozz::make_span(anim.ModelMatrices);

    if (!ltm.Run()) {
      LF_CORE_ERROR("LocalToModel failed");
      return;
    }

    anim.HasPose = true;
    // Skinning matrices
    const auto &inverseBind = anim.ModelRef->GetInverseBindMatrices();

    for (size_t i = 0; i < anim.ModelMatrices.size(); i++) {
      anim.FinalMatrices[i] = anim.ModelRef->GetRootInverse() * anim.ModelMatrices[i] * inverseBind[i];
    }
  });
}
} // namespace LevyeForge