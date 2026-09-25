#pragma once
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include <memory>
#include <string>

namespace LevyeForge {
// Clips are immutable and shared; each animator owns its sampling context.
class Animation {
public:
  Animation() = default;
  explicit Animation(const std::string &path, const ozz::animation::Skeleton *skeleton,
                     int clipIndex = 0);
  ozz::animation::Animation *Get() const { return m_Animation.get(); }
  const std::string &GetPath() const { return m_Path; }
  const std::string &GetError() const { return m_Error; }
  int GetClipIndex() const { return m_ClipIndex; }
  const ozz::animation::Skeleton *GetSkeleton() const { return m_Skeleton; }
private:
  std::shared_ptr<ozz::animation::Animation> m_Animation;
  const ozz::animation::Skeleton *m_Skeleton = nullptr;
  std::string m_Path, m_Error;
  int m_ClipIndex = 0;
};
}
