#pragma once
#include "ScriptableEntity.h"
#include <map>
#include <string>

namespace LevyeForge {
// Register compiled C++ scripts once, before loading a scene.
class NativeScriptRegistry {
public:
  using Factory = ScriptableEntity *(*)();
  template<class T> static void Register(const std::string &name) {
    Factories()[name] = []() -> ScriptableEntity * { return new T(); };
  }
  static const std::map<std::string, Factory> &Types() { return Factories(); }
  static bool Bind(NativeScriptComponent &component, const std::string &name) {
    if (component.Instance) return false;
    component.ClassName = name;
    component.Error.clear();
    const auto it = Factories().find(name);
    component.InstantiateScript = it == Factories().end() ? nullptr : it->second;
    component.DestroyScript = [](NativeScriptComponent *c) { delete c->Instance; c->Instance = nullptr; };
    return component.InstantiateScript != nullptr;
  }
private:
  static std::map<std::string, Factory> &Factories() {
    static std::map<std::string, Factory> types;
    return types;
  }
};
}
