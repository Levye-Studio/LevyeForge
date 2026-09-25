#include "Renderer/Model.h"
#include "AssimpToOzzBuilder.h"
#include "Log.h"
#include "Renderer/AssimpToGlm.h"
#include "lfpch.h"
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <stb_image.h>

// temp
#include <glad/glad.h>
using namespace std;
namespace LevyeForge {

unsigned int TextureFromFile(const char *path, const string &directory) {
  const auto filename = (std::filesystem::path(directory) / path).string();
  int width, height, channels;
  // Assimp already flips the UVs. Flipping these pixels again inverts the model.
  stbi_set_flip_vertically_on_load_thread(0);
  unsigned char *data = stbi_load(filename.c_str(), &width, &height, &channels, STBI_rgb_alpha);
  unsigned int textureID = 0;
  glGenTextures(1, &textureID);
  GLint previousBinding = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousBinding);
  glBindTexture(GL_TEXTURE_2D, textureID);
  if (data) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
  } else {
    LF_CORE_WARN("Texture failed to load: {}", filename);
    // A complete neutral texture avoids sampling an unallocated GL object.
    const unsigned char white[] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
  }
  glGenerateMipmap(GL_TEXTURE_2D);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glBindTexture(GL_TEXTURE_2D, previousBinding);
  return textureID;
}

Model::Model(const std::string &path, bool gamma)
    : m_GammaCorrection(gamma), m_Path(path) {
  LoadModel(path);
}

void Model::Draw(const Ref<Shader> &shader) {
  for (auto &mesh : m_Meshes)
    mesh->Draw(shader);
}

void Model::LoadModel(const std::string &path) {
  LF_PROFILE_FUNCTION();
  // read file via ASSIMP
  Assimp::Importer importer;
  importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, true);
  const aiScene *scene = importer.ReadFile(
      path, aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
  // error checking
  if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
      !scene->mRootNode) {
    LF_CORE_ERROR("Assimp Error: {0} {1}", importer.GetErrorString(), path);
    return;
  }
  m_Directory = std::filesystem::path(path).parent_path().string();
  // Vertex bone IDs and animation palettes must use the same joint ordering.
  m_Skeleton = AssimpSkeletonBuilder::Build(scene);
  if (!m_Skeleton)
    return;
  const int joints = m_Skeleton->num_joints();
  for (int i = 0; i < joints; ++i)
    m_JointMap[m_Skeleton->joint_names()[i]] = i;
  const glm::mat4 rootInverse = glm::inverse(AssimpGLMHelpers::ConvertMatrixToGLMFormat(scene->mRootNode->mTransformation));
  for (int column = 0; column < 4; ++column)
    m_RootInverse.cols[column] = ozz::math::simd_float4::Load(rootInverse[column][0], rootInverse[column][1], rootInverse[column][2], rootInverse[column][3]);
  ProcessNode(scene->mRootNode, scene);

  if (m_SkinningJoints.size() > 100) {
    LF_CORE_ERROR("Model {} exceeds the supported 100 deforming bones", m_Path);
    m_Meshes.clear();
    return;
  }
  m_InverseBindMatrices.resize(joints);

  ozz::math::Float4x4 identity;

  identity.cols[0] = ozz::math::simd_float4::Load(1, 0, 0, 0);
  identity.cols[1] = ozz::math::simd_float4::Load(0, 1, 0, 0);
  identity.cols[2] = ozz::math::simd_float4::Load(0, 0, 1, 0);
  identity.cols[3] = ozz::math::simd_float4::Load(0, 0, 0, 1);

  for (int i = 0; i < joints; i++) {
    std::string jointName = m_Skeleton->joint_names()[i];

    // // LF_CORE_TRACE("JointName: {}", jointName);
    // // LF_CORE_TRACE("boneinfomap size: {} joint size {}",
    // m_BoneInfoMap.size(),
    //               m_Skeleton->joint_names().size());

    auto it = m_BoneInfoMap.find(jointName);

    if (it == m_BoneInfoMap.end()) {
      // joint doesn't influence mesh
      m_InverseBindMatrices[i] = identity;
      continue;
    }

    glm::mat4 &offset = it->second.offset;

    ozz::math::Float4x4 mat;

    for (int column = 0; column < 4; ++column)
      mat.cols[column] = ozz::math::simd_float4::Load(offset[column][0], offset[column][1], offset[column][2], offset[column][3]);

    m_InverseBindMatrices[i] = mat;
  }

  // for (auto &m : m_InverseBindMatrices)
  //   m = identity;

  for (int i = 0; i < m_Skeleton->num_joints(); i++) {
    m_JointMap[m_Skeleton->joint_names()[i]] = i;
  }
}

void Model::ProcessNode(aiNode *node, const aiScene *scene) {
  LF_PROFILE_FUNCTION();
  for (unsigned int i = 0; i < node->mNumMeshes; i++) {
    aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
    m_Meshes.push_back(ProcessMesh(mesh, scene));
  }

  for (unsigned int i = 0; i < node->mNumChildren; i++) {
    ProcessNode(node->mChildren[i], scene);
  }
}

void Model::SetVertexBoneDataToDefault(Vertex &vertex) {
  for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
    vertex.m_BoneIDs[i] = -1;
    vertex.m_Weights[i] = 0.0f;
  }
}

Ref<Mesh> Model::ProcessMesh(aiMesh *mesh, const aiScene *scene) {
  LF_PROFILE_FUNCTION();
  Ref<Mesh> tempMesh;
  std::vector<Vertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<TextureMesh> textures;

  for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
    Vertex ivertex{};
    SetVertexBoneDataToDefault(ivertex);
    ivertex.Position = AssimpGLMHelpers::GetGLMVec(mesh->mVertices[i]);
    ivertex.Normal = mesh->HasNormals()
                         ? AssimpGLMHelpers::GetGLMVec(mesh->mNormals[i])
                         : glm::vec3(0.0f);
    ivertex.TexCoords = mesh->mTextureCoords[0]
                            ? glm::vec2(mesh->mTextureCoords[0][i].x,
                                        mesh->mTextureCoords[0][i].y)
                            : glm::vec2(0.0f);
    ivertex.Tangent =
        mesh->mTangents ? glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y,
                                    mesh->mTangents[i].z)
                        : glm::vec3(0.0f);
    ivertex.Bitangent = mesh->mBitangents ? glm::vec3(mesh->mBitangents[i].x,
                                                      mesh->mBitangents[i].y,
                                                      mesh->mBitangents[i].z)
                                          : glm::vec3(0.0f);
    vertices.push_back(ivertex);
  }

  for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
    aiFace face = mesh->mFaces[i];
    for (unsigned int j = 0; j < face.mNumIndices; j++) {
      indices.push_back(face.mIndices[j]);
    }
  }

  // process materials
  aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
  // we assume a convention for sampler names in the shaders. Each diffuse
  // texture should be named as 'texture_diffuseN' where N is a sequential
  // number ranging from 1 to MAX_SAMPLER_NUMBER. Same applies to other texture
  // as the following list summarizes: diffuse: texture_diffuseN specular:
  // texture_specularN normal: texture_normalN

  // 1. diffuse maps
  std::vector<TextureMesh> diffuseMaps =
      LoadMaterialTextures(material, aiTextureType_DIFFUSE, "texture_diffuse");
  textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
  // 2. specular maps
  std::vector<TextureMesh> specularMaps = LoadMaterialTextures(
      material, aiTextureType_SPECULAR, "texture_specular");
  textures.insert(textures.end(), specularMaps.begin(), specularMaps.end());
  // 3. normal maps
  std::vector<TextureMesh> normalMaps =
      LoadMaterialTextures(material, aiTextureType_HEIGHT, "texture_normal");
  textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
  // 4. height maps
  std::vector<TextureMesh> heightMaps =
      LoadMaterialTextures(material, aiTextureType_AMBIENT, "texture_height");
  textures.insert(textures.end(), heightMaps.begin(), heightMaps.end());

  LF_CORE_WARN("Mesh with {0} vertices and {1} indices", vertices.size(),
               indices.size());

  ExtractBoneWeightForVertices(vertices, mesh, scene);
  for (auto &vertex : vertices) {
    float sum = 0;
    for (float weight : vertex.m_Weights) sum += weight;
    if (sum > 0)
      for (float &weight : vertex.m_Weights) weight /= sum;
  }

  tempMesh = CreateRef<Mesh>(vertices, indices, textures);
  return tempMesh;
}

void Model::SetVertexBoneData(Vertex &vertex, int boneID, float weight) {
  if (!std::isfinite(weight) || weight <= 0)
    return;
  // Retain the strongest four influences, regardless of Assimp bone order.
  int slot = 0;
  for (int i = 0; i < MAX_BONE_INFLUENCE; ++i) {
    if (vertex.m_BoneIDs[i] < 0) { slot = i; break; }
    if (vertex.m_Weights[i] < vertex.m_Weights[slot]) slot = i;
  }
  if (vertex.m_BoneIDs[slot] < 0 || weight > vertex.m_Weights[slot]) {
    vertex.m_BoneIDs[slot] = boneID;
    vertex.m_Weights[slot] = weight;
  }
}

void Model::ExtractBoneWeightForVertices(std::vector<Vertex> &vertices,
                                         aiMesh *mesh, const aiScene *scene) {
  LF_PROFILE_FUNCTION();
  auto &boneInfoMap = m_BoneInfoMap;
  int &boneCount = m_BoneCounter;

  for (int boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
    int boneID = -1;
    std::string boneName = mesh->mBones[boneIndex]->mName.C_Str();

    auto joint = m_JointMap.find(boneName);
    if (joint == m_JointMap.end()) {
      LF_CORE_ERROR("Bone {} is missing from the model skeleton", boneName);
      continue;
    }
    if (boneInfoMap.find(boneName) == boneInfoMap.end()) {

      BoneInfo newBoneInfo;
      newBoneInfo.id = joint->second;
      newBoneInfo.paletteIndex = static_cast<int>(m_SkinningJoints.size());
      m_SkinningJoints.push_back(joint->second);
      newBoneInfo.offset = AssimpGLMHelpers::ConvertMatrixToGLMFormat(
          mesh->mBones[boneIndex]->mOffsetMatrix);
      boneInfoMap[boneName] = newBoneInfo;
      boneID = newBoneInfo.paletteIndex;
      boneCount++;

    } else {
      boneID = boneInfoMap[boneName].paletteIndex;
    }
    assert(boneID != -1);
    auto weights = mesh->mBones[boneIndex]->mWeights;
    int numWeights = mesh->mBones[boneIndex]->mNumWeights;

    for (int weightIndex = 0; weightIndex < numWeights; ++weightIndex) {
      int vertexId = weights[weightIndex].mVertexId;
      float weight = weights[weightIndex].mWeight;
      if (vertexId >= vertices.size()) {
        std::cerr << "Invalid vertex ID in bone weights: " << vertexId
                  << " >= " << vertices.size() << "\n";
        continue; // Or throw an error
      }
      assert(vertexId <= vertices.size());
      SetVertexBoneData(vertices[vertexId], boneID, weight);
    }
  }
}

std::vector<TextureMesh> Model::LoadMaterialTextures(aiMaterial *mat,
                                                     aiTextureType type,
                                                     std::string typeName) {
  LF_PROFILE_FUNCTION();
  std::vector<TextureMesh> textures;
  for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
    aiString str;
    mat->GetTexture(type, i, &str);
    if (str.length == 0 || str.C_Str() == nullptr) {
      std::cerr << "Invalid texture path!" << std::endl;
      continue;
    }
    // check if texture was loaded before and if so, continue to next iteration:
    // skip loading a new texture
    bool skip = false;
    for (unsigned int j = 0; j < m_TexturesLoaded.size(); j++) {
      if (std::strcmp(m_TexturesLoaded[j].path.data(), str.C_Str()) == 0) {
        textures.push_back(m_TexturesLoaded[j]);
        skip = true; // a texture with the same filepath has already been
                     // loaded, continue to next one. (optimization)
        break;
      }
    }

    if (!skip) {
      // if texture hasn't been loaded already, load it
      std::string tempPath = str.C_Str();
      // Texture2D texture = Texture2D((m_Directory + "/" + tempPath));
      TextureMesh texture;
      texture.id = TextureFromFile(str.C_Str(), m_Directory);
      texture.type = typeName;
      texture.path = str.C_Str();
      textures.push_back(texture);
      m_TexturesLoaded.push_back(texture);
      LF_CORE_INFO("Loaded tex type {0} path {1}", texture.type, str.C_Str());
      // if(texture.IsLoaded()){
      //     textures.push_back(texture);
      //     m_TexturesLoaded.push_back(texture);
      //     LF_CORE_INFO("Texture Loaded {0} with type {1}",
      //     texture.GetRendererID(), texture.type);
      // }
      // else{
      //     // textures.push_back(whiteTex);
      //     // m_TexturesLoaded.push_back(whiteTex);
      //     LF_CORE_INFO("Texture not Loaded {0} with type {1}",
      //     texture.GetRendererID(), texture.type);
      // }
    }
  }

  // if(!textures.empty())
  return textures;
}

} // namespace LevyeForge