#ifndef BAKER_HPP
#define BAKER_HPP

#include <filesystem>
#include <optional>
#include <tiny_gltf.h>
#include <glm/glm.hpp>

// struct Vertex {
//     float pos[3];
//     uint8_t normal[3];
//     uint8_t padding1;
//     float texcoord[2];
//     uint8_t tangent[3];
//     uint8_t padding2[5];
// };
struct Vertex
{
    // First 3 floats are position, 4th float is a packed normal
    glm::vec4 positionAndNormal;
    // First 2 floats are tex coords, 3rd is a packed tangent, 4th is padding
    glm::vec4 texCoordAndTangentAndPadding;
};

struct RenderElement
{
  std::uint32_t vertexOffset;
  std::uint32_t indexOffset;
  std::uint32_t indexCount;
};

struct Mesh
{
  std::uint32_t firstRelem;
  std::uint32_t relemCount;
};

struct ProcessedMeshes
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<RenderElement> relems;
    std::vector<Mesh> meshes;
};

class Baker {
public:
    void bakeScene(std::filesystem::path& path);
    ProcessedMeshes processMeshes(const tinygltf::Model& model);
    void updateBuffer(tinygltf::Model& model, ProcessedMeshes meshes, std::filesystem::path& path);
    void updateBufferViews(tinygltf::Model& model, ProcessedMeshes meshes);
    void updateAccessors(tinygltf::Model& model, ProcessedMeshes meshes);
    std::optional<tinygltf::Model> loadModel(const std::filesystem::path& path);
    uint8_t quantizeNormal(float value);
};

#endif
