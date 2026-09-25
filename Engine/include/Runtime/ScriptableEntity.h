#pragma once

#include "Entity.h"
#include "Auxiliaries/Physics.h"

namespace LevyeForge {

	class ScriptableEntity
	{
	public:
		virtual ~ScriptableEntity() {}

		template<typename T>
		T& GetComponent()
		{
			return m_Entity.GetComponent<T>();
		}

        template<typename T> bool HasComponent() { return m_Entity.HasComponent<T>(); }
        void DestroyEntity() { m_Scene->DestroyEntity(m_Entity); }

        Scene* GetScene(){
            return m_Scene;
        }

        JPH::BodyInterface &GetBodyInterface() { return *m_BodyInterface;}
	protected:
		virtual void OnCreate() {}
		virtual void OnDestroy() {}
		virtual void OnUpdate(Timestep ts) {}
        virtual void OnLateUpdate(Timestep ts) {}
		virtual void OnImGuiRender() {}
	private:
		Entity m_Entity;
        Scene* m_Scene = nullptr;
        JPH::BodyInterface *m_BodyInterface = nullptr;
		friend class Scene;
		friend class RuntimeScene;
		friend class SceneHierarchyPanel;
	};

}

