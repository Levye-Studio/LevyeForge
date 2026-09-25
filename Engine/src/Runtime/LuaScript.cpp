#include "Runtime/LuaScript.h"
#include "Auxiliaries/Physics.h"
#include "Core/Input.h"
#include "Core/KeyCodes.h"
#include "Core/Log.h"
#include "Runtime/RuntimeScene.h"
#include "lfpch.h"
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace LevyeForge {
namespace {
Entity &Self(lua_State *state) {
  auto *entity =
      static_cast<Entity *>(lua_touserdata(state, lua_upvalueindex(1)));
  if (!entity || !*entity)
    luaL_error(state, "Script entity no longer exists");
  return *entity;
}
glm::vec3 Vector(lua_State *state) {
  glm::vec3 value;
  for (int i = 0; i < 3; ++i) {
    value[i] = static_cast<float>(luaL_checknumber(state, i + 2));
    if (!std::isfinite(value[i]))
      luaL_error(state, "Expected finite coordinates");
  }
  return value;
}
int PushVector(lua_State *state, glm::vec3 value) {
  lua_pushnumber(state, value.x);
  lua_pushnumber(state, value.y);
  lua_pushnumber(state, value.z);
  return 3;
}
int GetPosition(lua_State *state) {
  return PushVector(state,
                    Self(state).GetComponent<TransformComponent>().Translation);
}
int GetRotation(lua_State *state) {
  return PushVector(state,
                    Self(state).GetComponent<TransformComponent>().Rotation);
}
int GetScale(lua_State *state) {
  return PushVector(state,
                    Self(state).GetComponent<TransformComponent>().Scale);
}
int SetPosition(lua_State *state) {
  auto &entity = Self(state);
  const auto position = Vector(state);
  entity.GetComponent<TransformComponent>().Translation = position;
  if (entity.HasComponent<RigidbodyComponent>()) {
    auto &body = entity.GetComponent<RigidbodyComponent>();
    if (body.RuntimeCreated) {
      const auto world=glm::vec3(entity.GetWorldTransform()[3]);
      Physics::GetSystem().GetBodyInterface().SetPosition(
          JPH::BodyID(body.BodyID), {world.x, world.y, world.z},
          JPH::EActivation::Activate);
    }
  }
  return 0;
}
int SetRotation(lua_State *state) {
  auto &entity = Self(state);
  const auto rotation = Vector(state);
  entity.GetComponent<TransformComponent>().Rotation = rotation;
  if (entity.HasComponent<RigidbodyComponent>()) {
    auto &body = entity.GetComponent<RigidbodyComponent>();
    if (body.RuntimeCreated) {
      const auto q = glm::quat(entity.GetScene()->GetWorldTransformComponents(entity).Rotation);
      Physics::GetSystem().GetBodyInterface().SetRotation(
          JPH::BodyID(body.BodyID), {q.x, q.y, q.z, q.w},
          JPH::EActivation::Activate);
    }
  }
  return 0;
}
int SetScale(lua_State *state) {
  auto &entity = Self(state);
  const auto scale = Vector(state);
  if (glm::any(glm::lessThan(glm::abs(scale), glm::vec3(0.001f))))
    return luaL_error(state, "Scale axes must be nonzero");
  entity.GetComponent<TransformComponent>().Scale = scale;
  if (auto *runtime = dynamic_cast<RuntimeScene *>(entity.GetScene());
      runtime && runtime->IsRunning()) {
    Physics::DestroyBody(entity, runtime);
    Physics::CreateBody(entity, runtime);
  }
  return 0;
}
int SetVelocity(lua_State *state) {
  auto &entity = Self(state);
  const auto velocity = Vector(state);
  if (!entity.HasComponent<RigidbodyComponent>())
    return luaL_error(state, "set_velocity requires a Rigid Body component");
  auto &body = entity.GetComponent<RigidbodyComponent>();
  if (body.Type != RigidbodyComponent::BodyType::Dynamic)
    return luaL_error(state, "set_velocity requires a Dynamic body; move "
                             "Kinematic bodies with set_position");
  body.LinearVelocity = velocity;
  if (body.RuntimeCreated && body.Type != RigidbodyComponent::BodyType::Static)
    Physics::GetSystem().GetBodyInterface().SetLinearVelocity(
        JPH::BodyID(body.BodyID), {velocity.x, velocity.y, velocity.z});
  return 0;
}
int Destroy(lua_State *state) {
  auto &entity = Self(state);
  entity.GetScene()->DestroyEntity(entity);
  return 0;
}
int PlayAnimation(lua_State *state) {
  auto &entity = Self(state);
  const char *path = luaL_checkstring(state, 2);
  int clip = static_cast<int>(luaL_optinteger(state, 3, 0));
  if (!entity.HasComponent<AnimatorComponent>())
    return luaL_error(state, "play_animation requires an Animator component");
  auto &animator = entity.GetComponent<AnimatorComponent>();
  Animation animation(
      path, animator.ModelRef ? animator.ModelRef->GetSkeleton() : nullptr,
      clip);
  bool success = animator.Play(animation);
  lua_pushboolean(state, success);
  lua_pushstring(state, success ? "" : animation.GetError().c_str());
  return 2;
}
int KeyDown(lua_State *state) {
  const auto key = luaL_checkinteger(state, 1);
  if (key < 0 || key >= 512)
    return luaL_error(state, "Key code is out of range");
  lua_pushboolean(state, Input::IsKeyPressed(static_cast<KeyCode>(key)));
  return 1;
}
int LogMessage(lua_State *state) {
  const char *message = luaL_checkstring(state, 1);
  LF_CORE_INFO("[Lua] {}", message);
  return 0;
}
void InstructionLimit(lua_State *state, lua_Debug *) {
  luaL_error(state, "Script exceeded the instruction budget for one callback");
}
} // namespace
LuaScriptInstance::LuaScriptInstance(Entity entity) : m_Entity(entity) {}
LuaScriptInstance::~LuaScriptInstance() {
  if (m_State)
    lua_close(m_State);
}
bool LuaScriptInstance::Fail() {
  const char *message = lua_tostring(m_State, -1);
  m_Error = message ? message : "Lua raised a non-string error";
  LF_CORE_ERROR("[Lua] {}", m_Error);
  lua_settop(m_State, 0);
  lua_sethook(m_State, nullptr, 0, 0);
  return false;
}
bool LuaScriptInstance::Call(const char *function, bool withTime, float time) {
  lua_getglobal(m_State, function);
  if (lua_isnil(m_State, -1)) {
    lua_pop(m_State, 1);
    return true;
  }
  if (withTime)
    lua_pushnumber(m_State, time);
  lua_sethook(m_State, InstructionLimit, LUA_MASKCOUNT, 1000000);
  if (lua_pcall(m_State, withTime ? 1 : 0, 0, 0) != LUA_OK)
    return Fail();
  lua_sethook(m_State, nullptr, 0, 0);
  return true;
}
bool LuaScriptInstance::Load(const std::string &path) {
  if (m_State)
    return false;
  m_State = luaL_newstate();
  if (!m_State) {
    m_Error = "Could not allocate Lua state";
    return false;
  }
  luaL_openlibs(m_State);
  lua_newtable(m_State);
  const luaL_Reg methods[] = {
      {"get_position", GetPosition},     {"set_position", SetPosition},
      {"get_rotation", GetRotation},     {"set_rotation", SetRotation},
      {"get_scale", GetScale},           {"set_scale", SetScale},
      {"set_velocity", SetVelocity},     {"destroy", Destroy},
      {"play_animation", PlayAnimation}, {nullptr, nullptr}};
  for (const auto *method = methods; method->name; ++method) {
    lua_pushlightuserdata(m_State, &m_Entity);
    lua_pushcclosure(m_State, method->func, 1);
    lua_setfield(m_State, -2, method->name);
  }
  lua_setglobal(m_State, "entity");
  lua_newtable(m_State);
  lua_pushcfunction(m_State, KeyDown);
  lua_setfield(m_State, -2, "is_key_down");
  lua_setglobal(m_State, "input");
  lua_newtable(m_State);
  for (int key = 'A'; key <= 'Z'; ++key) {
    char name[] = {static_cast<char>(key), 0};
    lua_pushinteger(m_State, key);
    lua_setfield(m_State, -2, name);
  }
  lua_pushinteger(m_State, Key::Space);
  lua_setfield(m_State, -2, "Space");
  lua_pushinteger(m_State, Key::LeftShift);
  lua_setfield(m_State, -2, "LeftShift");
  lua_setglobal(m_State, "Key");
  lua_pushcfunction(m_State, LogMessage);
  lua_setglobal(m_State, "log");
  lua_sethook(m_State, InstructionLimit, LUA_MASKCOUNT, 1000000);
  if (luaL_loadfile(m_State, path.c_str()) != LUA_OK ||
      lua_pcall(m_State, 0, 0, 0) != LUA_OK)
    return Fail();
  lua_sethook(m_State, nullptr, 0, 0);
  m_Started = true;
  return Call("on_create");
}
bool LuaScriptInstance::Update(float timestep) {
  return m_Started && m_Error.empty() && Call("on_update", true, timestep);
}
void LuaScriptInstance::Stop() {
  if (!m_Started)
    return;
  m_Started = false;
  Call("on_destroy");
}
} // namespace LevyeForge
