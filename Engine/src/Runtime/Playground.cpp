#include "lfpch.h"
#include "Runtime/Playground.h"
#include "Runtime/NativeScriptRegistry.h"
#include "Audio/Audio.h"
#include "Core/Input.h"
#include <imgui.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <glm/gtc/constants.hpp>

namespace LevyeForge {
namespace {
constexpr float HalfHeight=0.92f;
constexpr glm::vec3 Spawn{0, HalfHeight+0.05f, 6};
Entity Find(Scene &scene, const char *name) {
  for (auto id : scene.GetRegistry().view<TagComponent>())
    if (scene.GetRegistry().get<TagComponent>(id).Tag == name) return Entity(id,&scene);
  return {};
}
Entity Box(Scene &scene, const std::string &name, glm::vec3 position, glm::vec3 size,
           glm::vec3 color, bool solid=true) {
  auto entity=scene.CreateEntity(name);
  auto &transform=entity.GetComponent<TransformComponent>();
  transform.Translation=position; transform.Scale=size;
  entity.AddComponent<CubeComponent>().Color=color;
  if (solid) { entity.AddComponent<RigidbodyComponent>(); entity.AddComponent<BoxColliderComponent>(); }
  return entity;
}
struct GroundFilter : JPH::IgnoreSingleBodyFilter {
  using JPH::IgnoreSingleBodyFilter::IgnoreSingleBodyFilter;
  bool ShouldCollideLocked(const JPH::Body &body) const override { return !body.IsSensor(); }
};
}
void Playground::RegisterScripts() { NativeScriptRegistry::Register<PlaygroundController>("Playground Controller"); }
void Playground::Populate(Scene &scene) {
  RegisterScripts();
  Box(scene,"Courtyard",{0,-0.4f,-4},{20,0.8f,28},{0.16f,0.21f,0.27f});
  Box(scene,"West Wall",{-10,0.5f,-4},{0.4f,1,28},{0.28f,0.35f,0.43f});
  Box(scene,"East Wall",{10,0.5f,-4},{0.4f,1,28},{0.28f,0.35f,0.43f});
  Box(scene,"North Wall",{0,0.5f,-18},{20,1,0.4f},{0.28f,0.35f,0.43f});
  Box(scene,"South Wall",{0,0.5f,10},{20,1,0.4f},{0.28f,0.35f,0.43f});
  Box(scene,"Platform One",{-4,0.3f,0},{3.5f,0.6f,3.5f},{0.12f,0.47f,0.55f});
  Box(scene,"Platform Two",{4,0.65f,-4},{3.5f,1.3f,3.5f},{0.15f,0.42f,0.59f});
  Box(scene,"Step to Three",{0,0.5f,-7},{3,1,2.5f},{0.22f,0.38f,0.54f});
  Box(scene,"Platform Three",{0,1.1f,-10},{3.5f,2.2f,3.5f},{0.3f,0.35f,0.6f});
  const glm::vec3 cells[]={{-4,1.25f,0},{4,1.95f,-4},{0,2.85f,-10}};
  for (int i=0;i<3;++i) {
    Box(scene,"Energy Cell "+std::to_string(i+1),cells[i],{0.45f,0.65f,0.45f},{0.15f,0.95f,0.78f},false);
    Box(scene,"Cell Plinth "+std::to_string(i+1),cells[i]-glm::vec3(0,0.5f,0),
        {0.8f,0.15f,0.8f},{0.06f,0.12f,0.17f},false);
  }
  Box(scene,"Beacon Base",{0,0.15f,-15},{3,0.3f,3},{0.3f,0.35f,0.43f});
  Box(scene,"Beacon",{0,1.2f,-15},{0.8f,1.8f,0.8f},{0.7f,0.3f,0.15f},false);
  for (int i=0;i<8;++i)
    Box(scene,"Path Marker "+std::to_string(i),{0,0.015f,5-i*1.1f},{0.18f,0.03f,0.5f},{0.4f,0.65f,0.64f},false);
  auto light=scene.CreateEntity("Courtyard Light");
  light.GetComponent<TransformComponent>().Translation={-4,12,8};
  light.AddComponent<LightComponent>().Color={1,0.96f,0.88f,1};
  auto camera=scene.CreateEntity("Playground Camera");
  auto &cc=camera.AddComponent<CameraComponent>();
  cc.Primary=true;
  cc.Camera.SetPerspective(glm::radians(55.0f),0.1f,100.0f);
  camera.GetComponent<TransformComponent>().Translation={0,5,13};
  camera.GetComponent<TransformComponent>().Rotation={-0.38f,0,0};
  auto player=scene.CreateEntity("Player Body");
  player.GetComponent<TransformComponent>().Translation=Spawn;
  auto &rb=player.AddComponent<RigidbodyComponent>();
  rb.Type=RigidbodyComponent::BodyType::Dynamic; rb.Mass=75; rb.LockRotation=true;
  auto &collider=player.AddComponent<BoxColliderComponent>();
  collider.HalfSize={0.33f,HalfHeight,0.33f}; collider.Friction=0; collider.Restitution=0;
  auto man=scene.CreateEntity("Man");
  auto &transform=man.GetComponent<TransformComponent>();
  transform.Translation=Spawn-glm::vec3(0,HalfHeight,0);
  transform.Scale=glm::vec3(0.09f); transform.Rotation.y=glm::pi<float>();
  auto model=CreateRef<Model>("Resources/Animations/Idle.fbx");
  if (!model->IsLoaded()) throw std::runtime_error("Playground requires Resources/Animations/Idle.fbx");
  man.AddComponent<ModelComponent>().ModelData=model;
  auto &anim=man.AddComponent<AnimatorComponent>();
  anim.InitFromModel(model); anim.InPlaceJoint="mixamorig:Hips";
  if (!anim.Play(Animation("Resources/Animations/Idle.fbx",model->GetSkeleton())))
    throw std::runtime_error("Could not load the playground idle animation");
  NativeScriptRegistry::Bind(player.AddComponent<NativeScriptComponent>(),"Playground Controller");

  const auto root=scene.CreateEntity("Power the Beacon");
  auto group=[&](const char *name, Entity parent) {
    auto entity=scene.CreateEntity(name);
    scene.SetParent(entity,parent,false);
    return entity;
  };
  const auto environment=group("Environment",root);
  const auto walls=group("Walls",environment);
  const auto platforms=group("Platforms",environment);
  const auto markers=group("Path Markers",environment);
  const auto objectives=group("Objectives",root);
  const auto cellsGroup=group("Energy Cells",objectives);
  const auto beaconGroup=group("Beacon Assembly",objectives);
  const auto lighting=group("Lighting",root);
  const auto playerGroup=group("Player",root);
  for (auto entity : scene.GetChildren({})) {
    if (entity==root) continue;
    const auto &name=entity.GetName();
    Entity parent=environment;
    if (name.find("Wall")!=std::string::npos) parent=walls;
    else if (name.find("Platform")==0 || name=="Step to Three") parent=platforms;
    else if (name.find("Path Marker")==0) parent=markers;
    else if (name.find("Energy Cell")==0 || name.find("Cell Plinth")==0) parent=cellsGroup;
    else if (name.find("Beacon")==0) parent=beaconGroup;
    else if (entity==light) parent=lighting;
    else if (entity==player || entity==camera) parent=playerGroup;
    else if (entity==man) continue;
    scene.SetParent(entity,parent,true);
  }
  scene.SetParent(man,player,true);
}
void PlaygroundController::OnCreate() {
  m_Visual=Find(*GetScene(),"Man"); m_Camera=Find(*GetScene(),"Playground Camera"); m_Beacon=Find(*GetScene(),"Beacon");
  if (!m_Visual || !m_Camera || !m_Beacon) throw std::runtime_error("Playground scene is missing Man, camera, or beacon");
  for (int i=0;i<3;++i) {
    m_Cells[i]=Find(*GetScene(),("Energy Cell "+std::to_string(i+1)).c_str());
    if (!m_Cells[i]) throw std::runtime_error("Playground energy cell is missing");
    m_CellPositions[i]=m_Cells[i].GetComponent<TransformComponent>().Translation;
  }
  const auto *skeleton=m_Visual.GetComponent<ModelComponent>().ModelData->GetSkeleton();
  m_Idle=Animation("Resources/Animations/Idle.fbx",skeleton);
  m_Run=Animation("Resources/Animations/Running.fbx",skeleton);
  m_Jump=Animation("Resources/Animations/Jump.fbx",skeleton);
  if (!m_Idle.Get() || !m_Run.Get() || !m_Jump.Get()) throw std::runtime_error("Playground animation assets are missing");
  Reset();
}
void PlaygroundController::Reset() {
  auto &rb=GetComponent<RigidbodyComponent>();
  auto &transform=GetComponent<TransformComponent>();
  transform.Translation=Spawn; transform.Rotation={0,0,0};
  if (rb.RuntimeCreated) {
    auto &bi=GetBodyInterface();
    bi.SetPositionAndRotation(JPH::BodyID(rb.BodyID),ToJoltVec3(Spawn),JPH::Quat::sIdentity(),JPH::EActivation::Activate);
    bi.SetLinearVelocity(JPH::BodyID(rb.BodyID),JPH::Vec3::sZero());
  }
  Collected=0; Complete=false; Elapsed=0; m_Time=0; m_Collected.fill(false);
  m_Coyote=m_JumpBuffer=m_JumpCooldown=0;
  m_State=-1; m_Yaw=0; m_Pitch=0.38f; m_Facing=glm::pi<float>();
  for (int i=0;i<3;++i) if (m_Cells[i]) {
    auto &tc=m_Cells[i].GetComponent<TransformComponent>();
    tc.Translation=m_CellPositions[i]; tc.Scale={0.45f,0.65f,0.45f};
  }
  if (m_Beacon) m_Beacon.GetComponent<CubeComponent>().Color={0.7f,0.3f,0.15f};
  Prompt="Collect the three energy cells";
  OnLateUpdate(0);
}
bool PlaygroundController::ProbeGround() {
  const auto &rb=GetComponent<RigidbodyComponent>();
  if (!rb.RuntimeCreated) return false;
  const auto position=GetComponent<TransformComponent>().Translation;
  GroundFilter filter(JPH::BodyID(rb.BodyID));
  for (const glm::vec2 offset : {glm::vec2(0),glm::vec2(-0.24f,-0.24f),glm::vec2(0.24f,-0.24f),glm::vec2(-0.24f,0.24f),glm::vec2(0.24f,0.24f)}) {
    JPH::RRayCast ray({position.x+offset.x,position.y,position.z+offset.y},{0,-HalfHeight-0.09f,0});
    JPH::RayCastResult hit;
    if (!Physics::GetSystem().GetNarrowPhaseQuery().CastRay(ray,hit,{},{},filter)) continue;
    JPH::BodyLockRead lock(Physics::GetSystem().GetBodyLockInterface(),hit.mBodyID);
    if (lock.Succeeded() && lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2,ray.GetPointOnRay(hit.mFraction)).GetY()>0.6f)
      return true;
  }
  return false;
}
void PlaygroundController::OnUpdate(Timestep ts) {
  PlaygroundInput input;
  const glm::vec2 mouse=Input::GetMousePosition();
  if (InputEnabled) {
    input.Move={float(Input::IsKeyPressed(Key::D))-float(Input::IsKeyPressed(Key::A)),
                float(Input::IsKeyPressed(Key::W))-float(Input::IsKeyPressed(Key::S))};
    input.Jump=Input::IsKeyJustPressed(Key::Space);
    input.Interact=Input::IsKeyJustPressed(Key::E);
    input.Restart=Input::IsKeyJustPressed(Key::R);
    input.Sprint=Input::IsKeyPressed(Key::LeftShift);
    const bool orbit=Input::IsMouseButtonPressed(Mouse::ButtonRight);
    if (orbit && m_Orbiting) input.Look=(mouse-m_LastMouse)*0.004f;
    m_Orbiting=orbit;
    if (Input::IsKeyJustPressed(Key::M)) Audio::SetMuted(!Audio::IsMuted());
  } else m_Orbiting=false;
  m_LastMouse=mouse;
  Advance(input,std::clamp((float)ts,0.0f,0.1f));
}
void PlaygroundController::Advance(const PlaygroundInput &input, float dt) {
  if (input.Restart) { Reset(); return; }
  if (!Complete) Elapsed+=dt;
  m_Time+=dt;
  m_Yaw-=input.Look.x; m_Pitch=std::clamp(m_Pitch+input.Look.y,0.12f,0.95f);
  auto &rb=GetComponent<RigidbodyComponent>();
  if (!rb.RuntimeCreated) return;
  auto &bi=GetBodyInterface();
  auto velocity=bi.GetLinearVelocity(JPH::BodyID(rb.BodyID));
  m_JumpCooldown=std::max(0.0f,m_JumpCooldown-dt);
  Grounded=ProbeGround() && velocity.GetY()<0.5f && m_JumpCooldown<=0;
  m_Coyote=Grounded ? 0.10f : std::max(0.0f,m_Coyote-dt);
  m_JumpBuffer=input.Jump ? 0.12f : std::max(0.0f,m_JumpBuffer-dt);
  glm::vec2 move=input.Move;
  if (glm::length(move)>1) move=glm::normalize(move);
  if (Complete) move={0,0};
  const glm::vec3 forward{-std::sin(m_Yaw),0,-std::cos(m_Yaw)};
  const glm::vec3 right{std::cos(m_Yaw),0,-std::sin(m_Yaw)};
  const glm::vec3 desired=(right*move.x+forward*move.y)*(input.Sprint?6.0f:4.0f);
  float vertical=velocity.GetY();
  if (!Complete && m_JumpBuffer>0 && m_Coyote>0) {
    vertical=6.4f; m_JumpBuffer=m_Coyote=0; m_JumpCooldown=0.2f; Grounded=false;
    Audio::Play(SoundCue::Jump);
  }
  bi.SetLinearVelocity(JPH::BodyID(rb.BodyID),{desired.x,vertical,desired.z});
  if (glm::length(desired)>0.01f) m_Facing=std::atan2(desired.x,desired.z);
  auto &anim=m_Visual.GetComponent<AnimatorComponent>();
  const int next=!Grounded ? 2 : glm::length(move)>0.1f ? 1 : 0;
  if (next!=m_State) {
    anim.CrossFade(next==2?m_Jump:next==1?m_Run:m_Idle,0.16f);
    anim.Loop=next!=2;
    m_State=next;
  }
  anim.Speed=next==1 ? (input.Sprint?1.3f:0.9f) : 1;
  const glm::vec3 feet=GetComponent<TransformComponent>().Translation-glm::vec3(0,HalfHeight,0);
  Prompt=Collected<3 ? "Find the green energy cells" : "All cells collected - activate the beacon";
  for (int i=0;i<3;++i) {
    if (m_Collected[i] || !m_Cells[i]) continue;
    auto &tc=m_Cells[i].GetComponent<TransformComponent>();
    tc.Rotation.y=m_Time;
    tc.Translation=m_CellPositions[i]+glm::vec3(0,0.08f*std::sin(m_Time*2),0);
    const auto delta=feet-(m_CellPositions[i]-glm::vec3(0,0.65f,0));
    if (glm::length(glm::vec2(delta.x,delta.z))<1.3f && std::abs(delta.y)<0.45f) {
      Prompt="E  -  Collect energy cell";
      if (input.Interact && !Complete) {
        m_Collected[i]=true; ++Collected; tc.Translation.y=-100;
        Audio::Play(SoundCue::Collect);
      }
    }
  }
  if (Collected==3) m_Beacon.GetComponent<CubeComponent>().Color={0.15f,1,0.55f};
  const auto beaconDelta=feet-glm::vec3(0,0,-15);
  if (glm::length(glm::vec2(beaconDelta.x,beaconDelta.z))<2 && std::abs(beaconDelta.y)<0.6f) {
    Prompt=Collected==3 ? "E  -  Power the beacon" : "The beacon needs all 3 energy cells";
    if (input.Interact && Collected==3 && !Complete) { Complete=true; Audio::Play(SoundCue::Complete); }
  }
  if (Complete) Prompt="Beacon powered!  R - Play again";
  if (feet.y<-5) Reset();
}
void PlaygroundController::OnLateUpdate(Timestep) {
  if (!m_Visual || !m_Camera) return;
  const auto position=GetComponent<TransformComponent>().Translation;
  auto visual=GetScene()->GetWorldTransformComponents(m_Visual);
  visual.Translation=position-glm::vec3(0,HalfHeight,0);
  visual.Rotation.y=m_Facing;
  GetScene()->SetWorldTransform(m_Visual,visual.GetTransform());
  const glm::vec3 target=position+glm::vec3(0,0.35f,0);
  const glm::vec3 offset{std::sin(m_Yaw)*std::cos(m_Pitch),std::sin(m_Pitch),std::cos(m_Yaw)*std::cos(m_Pitch)};
  glm::vec3 cameraPosition=target+offset*6.5f;
  const auto &rb=GetComponent<RigidbodyComponent>();
  if (rb.RuntimeCreated) {
    GroundFilter filter(JPH::BodyID(rb.BodyID));
    JPH::RayCastResult hit;
    JPH::RRayCast ray(ToJoltVec3(target),ToJoltVec3(cameraPosition-target));
    if (Physics::GetSystem().GetNarrowPhaseQuery().CastRay(ray,hit,{},{},filter))
      cameraPosition=target+offset*std::max(1.0f,6.5f*hit.mFraction-0.25f);
  }
  auto &camera=m_Camera.GetComponent<TransformComponent>();
  camera.Translation=cameraPosition;
  camera.Rotation=glm::eulerAngles(glm::quat_cast(glm::inverse(glm::lookAt(cameraPosition,target,glm::vec3(0,1,0)))));
}
PlaygroundController *Playground::Controller(Scene &scene) {
  for (auto id : scene.GetRegistry().view<NativeScriptComponent>()) {
    auto &script=scene.GetRegistry().get<NativeScriptComponent>(id);
    if (auto *controller=dynamic_cast<PlaygroundController *>(script.Instance)) return controller;
  }
  return nullptr;
}
void Playground::DrawHUD(Scene &scene, glm::vec2 origin, glm::vec2 size) {
  auto *controller=Controller(scene);
  if (!controller || size.x<200 || size.y<150) return;
  auto *draw=ImGui::GetWindowDrawList();
  ImVec2 top{origin.x+18,origin.y+18};
  draw->PushClipRect({origin.x,origin.y},{origin.x+size.x,origin.y+size.y},true);
  draw->AddRectFilled(top,{top.x+std::min(400.0f,size.x-36),top.y+82},IM_COL32(12,20,28,220),9);
  draw->AddText({top.x+14,top.y+10},IM_COL32(100,245,207,255),"LEVYE FORGE / POWER THE BEACON");
  const auto progress="Energy cells: "+std::to_string(controller->Collected)+" / 3   |   "+std::to_string(int(controller->Elapsed))+"s";
  draw->AddText({top.x+14,top.y+37},IM_COL32(240,244,248,255),progress.c_str());
  draw->AddText({top.x+14,top.y+59},IM_COL32(170,190,205,255),Audio::IsMuted()?"M - Unmute":Audio::IsAvailable()?"M - Mute":"Audio unavailable");
  const float bottom=origin.y+size.y-88;
  draw->AddRectFilled({origin.x+18,bottom},{origin.x+size.x-18,origin.y+size.y-18},IM_COL32(12,20,28,220),9);
  draw->AddText({origin.x+32,bottom+10},controller->Complete?IM_COL32(100,245,207,255):IM_COL32(250,220,125,255),controller->Prompt.c_str());
  draw->AddText({origin.x+32,bottom+36},IM_COL32(225,232,240,255),"WASD Move  |  Space Jump  |  E Interact  |  Shift Sprint  |  RMB Look  |  R Restart");
  draw->PopClipRect();
}
}
