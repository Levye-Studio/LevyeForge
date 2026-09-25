#pragma once
#include "Entity.h"
struct lua_State;
namespace LevyeForge {
class LuaScriptInstance {
public:
  explicit LuaScriptInstance(Entity entity);
  ~LuaScriptInstance();
  bool Load(const std::string &path);
  bool Update(float timestep);
  void Stop();
  const std::string &GetError() const { return m_Error; }
private:
  bool Call(const char *function, bool withTime = false, float time = 0);
  bool Fail();
  lua_State *m_State = nullptr;
  Entity m_Entity;
  std::string m_Error;
  bool m_Started = false;
};
}
