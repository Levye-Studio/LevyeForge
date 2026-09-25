#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Core/Log.h"
#include "entity/fwd.hpp"
#include <Jolt/Jolt.h>

#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
namespace LevyeForge {

class RuntimeScene;

static inline glm::quat ToGlmQuat(const JPH::Quat &q) {
  return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ()); // glm wants
  //   (w, x, y, z)
}
static inline JPH::Quat ToJoltQuat(const glm::quat &q) {
  return JPH::Quat(q.x, q.y, q.z, q.w); // Jolt ctor is (x,y,z,w)
}

static inline JPH::Vec3 ToJoltVec3(const glm::vec3 &v) {
  return JPH::Vec3(v.x, v.y, v.z);
}

static inline glm::vec3 ToGlm(const JPH::Vec3 &v) {
  return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

static inline void FromJoltTransform(const JPH::RMat44 &xf, glm::vec3 &outPos,
                                     glm::vec3 &outEuler) {
  const JPH::RVec3 t = xf.GetTranslation();
  outPos = glm::vec3((float)t.GetX(), (float)t.GetY(), (float)t.GetZ());

  const JPH::Quat rq = xf.GetRotation().GetQuaternion(); // Jolt rotation
  const glm::quat gq((float)rq.GetW(), (float)rq.GetX(), (float)rq.GetY(),
                     (float)rq.GetZ());
  outEuler = glm::eulerAngles(gq); // radians
}

namespace BroadPhaseLayers {
static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
static constexpr JPH::BroadPhaseLayer MOVING(1);
static constexpr JPH::uint NUM_LAYERS(2);
}; // namespace BroadPhaseLayers

namespace Layers {
static constexpr JPH::ObjectLayer NON_MOVING = 0; // static/world
static constexpr JPH::ObjectLayer MOVING = 1;     // dynamic/kinematic
//     rigidbodies
static constexpr JPH::ObjectLayer CHARACTER = 2; //
//     player/NPC controllers
static constexpr JPH::ObjectLayer NUM_LAYERS = 3;
} // namespace Layers

// BroadPhaseLayerInterface implementation
// This defines a mapping between object and broadphase layers.
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
  BPLayerInterfaceImpl() {
    // Create a mapping table from object to broad phase layer
    mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
    mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
    mObjectToBroadPhase[Layers::CHARACTER] = BroadPhaseLayers::MOVING;
  }

  [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override {
    return BroadPhaseLayers::NUM_LAYERS;
  }

  [[nodiscard]] JPH::BroadPhaseLayer
  GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
    JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
    return mObjectToBroadPhase[inLayer];
  }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
  virtual const char *
  GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
    switch ((JPH::BroadPhaseLayer::Type)inLayer) {
    case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
      return "NON_MOVING";
    case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
      return "MOVING";
    default:
      JPH_ASSERT(false);
      return "INVALID";
    }
  }
#endif // JPH_EXTERNAL_PROFILE || JPH_PROFILE_ENABLED

private:
  JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
  //   std::array<JPH::BroadPhaseLayer, Layers::NUM_LAYERS> mObjectToBroadPhase;
};

class ObjectVsBroadPhaseLayerFilterImpl
    : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
  [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer ol,
                                   JPH::BroadPhaseLayer bpl) const override {
    switch (ol) {
    case Layers::NON_MOVING:
      return bpl == BroadPhaseLayers::MOVING;
    case Layers::MOVING:
      return true;
    case Layers::CHARACTER:
      return bpl == BroadPhaseLayers::NON_MOVING ||
             bpl == BroadPhaseLayers::MOVING;
    default:
      JPH_ASSERT(false);
      return false;
    }
  }
};

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter {
public:
  bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
    switch (a) {
    case Layers::NON_MOVING:
      return b == Layers::MOVING || b == Layers::CHARACTER;
    case Layers::MOVING:
      return true; //
    // collide with NM, MOVING, CHARACTER
    case Layers::CHARACTER:
      return b == Layers::NON_MOVING || b == Layers::MOVING; // (no char-char)
    default:
      JPH_ASSERT(false);
      return false;
    }
  }
};

// An example contact listener
class MyContactListener : public JPH::ContactListener {
public:
  // See: ContactListener
  JPH::ValidateResult OnContactValidate(
      const JPH::Body & /* inBody1 */, const JPH::Body & /* inBody2 */,
      JPH::RVec3Arg /* inBaseOffset */,
      const JPH::CollideShapeResult & /* inCollisionResult */) override {

    // Allows you to ignore a contact before it is created (using
    // layers to not
    // make objects collide is cheaper!)
    return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
  }

  void OnContactAdded(const JPH::Body & /* inBody1 */,
                      const JPH::Body & /* inBody2 */,
                      const JPH::ContactManifold & /* inManifold */,
                      JPH::ContactSettings & /* ioSettings */) override {
  }

  void OnContactPersisted(const JPH::Body & /* inBody1 */,
                          const JPH::Body & /* inBody2 */,
                          const JPH::ContactManifold & /* inManifold
                                                        */
                          ,
                          JPH::ContactSettings & /* ioSettings */) override {
  }

  void
  OnContactRemoved(const JPH::SubShapeIDPair & /* inSubShapePair */) override {
  }
};

// An example activation listener
class MyBodyActivationListener : public JPH::BodyActivationListener {
public:
  void OnBodyActivated(const JPH::BodyID & /* inBodyID */,
                       JPH::uint64 /* inBodyUserData */) override {
  }

  void OnBodyDeactivated(const JPH::BodyID & /* &inBodyID */,
                         JPH::uint64 /* inBodyUserData */) override {
  }
};

class Physics {
public:
  static void Init();
  static void Shutdown();

  static void Step(RuntimeScene *scene, float dt);

  static JPH::PhysicsSystem &GetSystem();

  static void CreateBody(entt::entity entity, RuntimeScene *scene);
  static void DestroyBody(entt::entity entity, RuntimeScene *scene);

private:
  static std::unique_ptr<BPLayerInterfaceImpl> _bp_iface;
  static std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> _obj_vs_bp_filter;
  static std::unique_ptr<ObjectLayerPairFilterImpl> _obj_vs_obj_filter;
  static std::unique_ptr<JPH::PhysicsSystem> m_PhysicsSystem;

  static std::unique_ptr<JPH::TempAllocatorImpl> _temp_alloc;
  static std::unique_ptr<JPH::JobSystemThreadPool> _jobs;

  // lifetime: these must outlive PhysicsSystem (Jolt keeps refs to them)
  static std::unique_ptr<MyContactListener> _contact_listener;
  static std::unique_ptr<MyBodyActivationListener> _activation_listener;
};
} // namespace LevyeForge