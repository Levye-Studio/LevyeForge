#pragma once

#include <LevyeForge.h>

namespace LevyeForge {

	class SceneHierarchyPanel
	{
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Ref<Scene>& scene);

		void SetContext(const Ref<Scene>& scene);

		void OnImGuiRender();

		Entity GetSelectedEntity() const { return m_SelectionContext; }
		void SetSelectedEntity(Entity entity);
	private:
		void DrawEntityNode(Entity entity);
        void AcceptEntityDrop(Entity parent);
		void DrawComponents(Entity entity);
	private:
		Ref<Scene> m_Context;
		Entity m_SelectionContext;
        std::vector<std::function<void()>> m_HierarchyActions;
        Entity m_ExpandEntity;
        std::string m_HierarchyError;
	};

}
