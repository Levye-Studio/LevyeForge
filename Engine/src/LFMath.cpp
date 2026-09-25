#include "lfpch.h"
#include "Math/LFMath.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

namespace LevyeForge::Math {
bool DecomposeTransform(const glm::mat4& transform, glm::vec3& translation,
                        glm::vec3& rotation, glm::vec3& scale) {
  for (int column = 0; column < 4; ++column)
    for (int row = 0; row < 4; ++row)
      if (!std::isfinite(transform[column][row])) return false;

  glm::vec3 position, dimensions, skew;
  glm::vec4 perspective;
  glm::quat orientation;
  if (!glm::decompose(transform, dimensions, orientation, position, skew, perspective) ||
      glm::any(glm::lessThan(glm::abs(dimensions), glm::vec3(1e-6f))))
    return false;
  const auto angles = glm::eulerAngles(glm::normalize(orientation));
  for (int axis = 0; axis < 3; ++axis)
    if (!std::isfinite(angles[axis])) return false;
  translation = position;
  rotation = angles;
  scale = dimensions;
  return true;
}
}
