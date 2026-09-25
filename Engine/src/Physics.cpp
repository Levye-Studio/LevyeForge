#include "Auxiliaries/Physics.h"
#include "Runtime/Components.h"
#include "Runtime/RuntimeScene.h"
#include <Jolt/Core/Factory.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/RegisterTypes.h>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace LevyeForge {

std::unique_ptr<BPLayerInterfaceImpl> Physics::_bp_iface;
std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> Physics::_obj_vs_bp_filter;
std::unique_ptr<ObjectLayerPairFilterImpl> Physics::_obj_vs_obj_filter;
std::unique_ptr<JPH::PhysicsSystem> Physics::m_PhysicsSystem;
std::unique_ptr<JPH::TempAllocatorImpl> Physics::_temp_alloc;
std::unique_ptr<JPH::JobSystemThreadPool> Physics::_jobs;
std::unique_ptr<MyContactListener> Physics::_contact_listener;
std::unique_ptr<MyBodyActivationListener> Physics::_activation_listener;

namespace {
constexpr float FixedStep = 1.0f / 60.0f;
float accumulator = 0.0f;
std::unordered_map<entt::entity, JPH::BodyID> bodies;
}

void Physics::Init() {
  if (m_PhysicsSystem)
    return;
  JPH::RegisterDefaultAllocator();
  JPH::Factory::sInstance = new JPH::Factory();
  JPH::RegisterTypes();
  _bp_iface = std::make_unique<BPLayerInterfaceImpl>();
  _obj_vs_bp_filter = std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
  _obj_vs_obj_filter = std::make_unique<ObjectLayerPairFilterImpl>();
  _contact_listener = std::make_unique<MyContactListener>();
  _activation_listener = std::make_unique<MyBodyActivationListener>();
  _temp_alloc = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024);
  const int workers = std::max(1, (int)std::thread::hardware_concurrency() - 1);
  _jobs = std::make_unique<JPH::JobSystemThreadPool>(1024, 8, workers);
  m_PhysicsSystem = std::make_unique<JPH::PhysicsSystem>();
  m_PhysicsSystem->Init(65536, 8, 65536, 10240, *_bp_iface,
                        *_obj_vs_bp_filter, *_obj_vs_obj_filter);
  m_PhysicsSystem->SetGravity(JPH::Vec3(0, -9.81f, 0));
  m_PhysicsSystem->SetBodyActivationListener(_activation_listener.get());
  m_PhysicsSystem->SetContactListener(_contact_listener.get());
  accumulator = 0.0f;
}

JPH::PhysicsSystem &Physics::GetSystem() {
  if (!m_PhysicsSystem)
    throw std::logic_error("Physics has not been started");
  return *m_PhysicsSystem;
}

void Physics::CreateBody(entt::entity entity, RuntimeScene *scene) {
  if (!m_PhysicsSystem || bodies.count(entity))
    return;
  auto &registry = scene->GetRegistry();
  if ((!registry.all_of<RigidbodyComponent, TransformComponent>(entity) ||
         !registry.any_of<BoxColliderComponent, SphereColliderComponent>(entity)))
    return;
  auto &rb = registry.get<RigidbodyComponent>(entity);
  auto transform = scene->GetWorldTransformComponents(entity);
  rb.RuntimeCreated = false;
  rb.BodyID = 0xffffffffu;
  JPH::ShapeSettings::ShapeResult result;
  float friction = 0.5f, restitution = 0.0f;
  bool isTrigger = false;
  if (auto *collider = registry.try_get<BoxColliderComponent>(entity)) {
    const glm::vec3 halfSize = collider->HalfSize * glm::abs(transform.Scale);
    if (!std::isfinite(halfSize.x) || !std::isfinite(halfSize.y) || !std::isfinite(halfSize.z) ||
        glm::any(glm::lessThanEqual(halfSize, glm::vec3(0)))) {
      LF_CORE_ERROR("Cannot create collider with non-positive or non-finite extents");
      return;
    }
    const float radius = std::min(0.05f, std::min({halfSize.x, halfSize.y, halfSize.z}) * 0.5f);
    result = JPH::BoxShapeSettings(ToJoltVec3(halfSize), radius).Create();
    friction = collider->Friction;
    restitution = collider->Restitution;
    isTrigger = collider->IsTrigger;
  } else {
    const auto &sphere = registry.get<SphereColliderComponent>(entity);
    const glm::vec3 scale = glm::abs(transform.Scale);
    const float radius = sphere.Radius * std::max({scale.x, scale.y, scale.z});
    if (!std::isfinite(radius) || radius <= 0) {
      LF_CORE_ERROR("Cannot create sphere with non-positive or non-finite radius");
      return;
    }
    result = JPH::SphereShapeSettings(radius).Create();
    friction = sphere.Friction;
    restitution = sphere.Restitution;
    isTrigger = sphere.IsTrigger;
  }
  if (result.HasError()) {
    LF_CORE_ERROR("Cannot create collider: {}", result.GetError().c_str());
    return;
  }
  JPH::EMotionType motion = JPH::EMotionType::Static;
  if (rb.Type == RigidbodyComponent::BodyType::Dynamic)
    motion = JPH::EMotionType::Dynamic;
  else if (rb.Type == RigidbodyComponent::BodyType::Kinematic)
    motion = JPH::EMotionType::Kinematic;
  const bool moving = motion != JPH::EMotionType::Static;
  JPH::BodyCreationSettings settings(result.Get(),
      JPH::RVec3(transform.Translation.x, transform.Translation.y, transform.Translation.z),
      ToJoltQuat(glm::quat(transform.Rotation)), motion,
      moving ? Layers::MOVING : Layers::NON_MOVING);
  if (rb.LockRotation)
    settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX | JPH::EAllowedDOFs::TranslationY | JPH::EAllowedDOFs::TranslationZ;
  settings.mLinearVelocity = ToJoltVec3(rb.LinearVelocity);
  settings.mGravityFactor = rb.UseGravity ? 1.0f : 0.0f;
  settings.mLinearDamping = std::clamp(rb.LinearDamping, 0.0f, 1.0f);
  settings.mAngularDamping = std::clamp(rb.AngularDamping, 0.0f, 1.0f);
  settings.mFriction = std::max(0.0f, friction);
  settings.mRestitution = std::clamp(restitution, 0.0f, 1.0f);
  settings.mIsSensor = isTrigger;
  if (moving) {
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = std::max(0.001f, rb.Mass);
  }
  auto id = m_PhysicsSystem->GetBodyInterface().CreateAndAddBody(settings,
      moving ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
  if (id.IsInvalid()) {
    LF_CORE_ERROR("Physics body capacity exceeded");
    return;
  }
  bodies.emplace(entity, id);
  rb.BodyID = id.GetIndexAndSequenceNumber();
  rb.RuntimeCreated = true;
}

void Physics::DestroyBody(entt::entity entity, RuntimeScene *scene) {
  auto it = bodies.find(entity);
  if (m_PhysicsSystem && it != bodies.end()) {
    auto &bi = m_PhysicsSystem->GetBodyInterface();
    bi.RemoveBody(it->second);
    bi.DestroyBody(it->second);
    bodies.erase(it);
  }
  if (auto *rb = scene->GetRegistry().try_get<RigidbodyComponent>(entity)) {
    rb->BodyID = 0xffffffffu;
    rb->RuntimeCreated = false;
  }
}

void Physics::Step(RuntimeScene *scene, float dt) {
  if (!m_PhysicsSystem || !std::isfinite(dt) || dt <= 0)
    return;
  auto &registry = scene->GetRegistry();
  // Also handle entities/components added or removed by runtime scripts.
  for (auto it = bodies.begin(); it != bodies.end();) {
    const auto entity = it->first;
    ++it;
    if (!registry.valid(entity) ||
        (!registry.all_of<RigidbodyComponent, TransformComponent>(entity) ||
         !registry.any_of<BoxColliderComponent, SphereColliderComponent>(entity)))
      DestroyBody(entity, scene);
  }
  auto view = registry.view<RigidbodyComponent, TransformComponent>();
  for (auto entity : view)
    CreateBody(entity, scene);

  accumulator += std::min(dt, 0.25f);
  auto &bi = m_PhysicsSystem->GetBodyInterface();
  while (accumulator >= FixedStep) {
    for (auto entity : view) {
      auto &rb = view.get<RigidbodyComponent>(entity);
      if (!rb.RuntimeCreated || rb.Type == RigidbodyComponent::BodyType::Dynamic)
        continue;
      auto transform = scene->GetWorldTransformComponents(entity);
      if (rb.Type == RigidbodyComponent::BodyType::Static) {
        bi.SetPositionAndRotation(JPH::BodyID(rb.BodyID),
            JPH::RVec3(transform.Translation.x, transform.Translation.y, transform.Translation.z),
            ToJoltQuat(glm::quat(transform.Rotation)), JPH::EActivation::DontActivate);
        continue;
      }
      bi.MoveKinematic(JPH::BodyID(rb.BodyID),
          JPH::RVec3(transform.Translation.x, transform.Translation.y, transform.Translation.z),
          ToJoltQuat(glm::quat(transform.Rotation)), FixedStep);
    }
    m_PhysicsSystem->Update(FixedStep, 1, _temp_alloc.get(), _jobs.get());
    accumulator -= FixedStep;
  }
  // Sync parent bodies first, then convert each simulated world pose to local space.
  struct BodyPose { entt::entity EntityID; glm::mat4 World; int Depth; };
  std::vector<BodyPose> poses;
  for (auto entity : view) {
    auto &rb=view.get<RigidbodyComponent>(entity);
    if (!rb.RuntimeCreated || rb.Type!=RigidbodyComponent::BodyType::Dynamic) continue;
    auto transform=scene->GetWorldTransformComponents(entity);
    const auto id=JPH::BodyID(rb.BodyID);
    const auto position=bi.GetPosition(id);
    transform.Translation={position.GetX(),position.GetY(),position.GetZ()};
    transform.Rotation=glm::eulerAngles(ToGlmQuat(bi.GetRotation(id)));
    int depth=0;
    for (auto parent=scene->GetParent(Entity(entity,scene)); parent; parent=scene->GetParent(parent)) ++depth;
    poses.push_back({entity,transform.GetTransform(),depth});
  }
  std::sort(poses.begin(),poses.end(),[](const auto &a,const auto &b) { return a.Depth<b.Depth; });
  for (const auto &pose : poses) scene->SetWorldTransform(pose.EntityID,pose.World);

}

void Physics::Shutdown() {
  if (!m_PhysicsSystem)
    return;
  // Destroy bodies and the system before the filters, listeners and Jolt factory.
  auto &bi = m_PhysicsSystem->GetBodyInterface();
  JPH::BodyIDVector ids;
  m_PhysicsSystem->GetBodies(ids);
  for (auto id : ids) {
    bi.RemoveBody(id);
    bi.DestroyBody(id);
  }
  bodies.clear();
  m_PhysicsSystem.reset();
  _jobs.reset();
  _temp_alloc.reset();
  _activation_listener.reset();
  _contact_listener.reset();
  _bp_iface.reset();
  _obj_vs_bp_filter.reset();
  _obj_vs_obj_filter.reset();
  JPH::UnregisterTypes();
  delete JPH::Factory::sInstance;
  JPH::Factory::sInstance = nullptr;
  accumulator = 0.0f;
}
} // namespace LevyeForge
