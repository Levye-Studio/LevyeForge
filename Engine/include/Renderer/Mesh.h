#pragma once

#include "Core/Config.h"
#include "Texture.h"
#include "Shader.h"
#include "Texture.h"
#include "VertexArray.h"

#define MAX_BONE_INFLUENCE 4

namespace LevyeForge {
    
    struct Vertex {
        glm::vec3 Position{0};
        glm::vec3 Normal{0, 1, 0};
        glm::vec2 TexCoords{0};
        glm::vec3 Tangent{1, 0, 0};
        glm::vec3 Bitangent{0, 0, 1};

        int m_BoneIDs[MAX_BONE_INFLUENCE] = {-1, -1, -1, -1};
        float m_Weights[MAX_BONE_INFLUENCE] = {};
        int EntityID = -1;
    };

    struct TextureMesh {
        unsigned int id;
        std::string type;
        std::string path;
    };


    class Mesh {
    public:
        Mesh(const std::vector<Vertex>& vertices,const std::vector<uint32_t>& indices,const std::vector<TextureMesh>& textures);

        void Draw(const Ref<Shader> &shader);

    public:
        //mesh data
        std::vector<Vertex> m_Vertices;
        std::vector<uint32_t> m_Indices;
        std::vector<TextureMesh> m_Textures;
        Ref<VertexArray> m_VertexArray;   
        unsigned int VAO;     
    private:        
        Ref<Texture2D> m_FallbackTexture;
        unsigned int VBO, EBO;
        void setupMesh();
    };
    
}