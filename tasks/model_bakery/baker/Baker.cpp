#include "Baker.hpp"
#include "etna/Assert.hpp"
#include <bit>
#include <cstdint>
#include <cstring>
#include <glm/fwd.hpp>
#include <optional>
#include <spdlog/spdlog.h>
#include <tiny_gltf.h>
#include <spdlog/spdlog.h>
#include <fmt/std.h>
#include <vector>

static std::uint32_t encode_normal(glm::vec4 normal)
{
  const std::int8_t x = static_cast<std::int8_t>(std::round(255 * 0.5f * (normal.x + 1.0f)));
  const std::int8_t y = static_cast<std::int8_t>(std::round(255 * 0.5f * (normal.y + 1.0f)));
  const std::int8_t z = static_cast<std::int8_t>(std::round(255 * 0.5f * (normal.z + 1.0f)));
  const std::int8_t w = static_cast<std::int8_t>(std::round(255 * 0.5f * (normal.w + 1.0f)));
  const int8_t combined[4] = { x, y, z, w };

  return std::bit_cast<uint32_t>(combined);
}

void Baker::bakeScene(std::filesystem::path& path)
{
  auto maybeModel = loadModel(path);
  if (!maybeModel.has_value())
    return;

  auto model = std::move(*maybeModel);
  model.extensionsRequired.push_back("KHR_mesh_quantization");
  model.extensionsUsed.push_back("KHR_mesh_quantization");

  auto result = processMeshes(model);

  spdlog::info("Updating model");
  updateBuffer(model, result, path);
  updateBufferViews(model, result);
  updateAccessors(model, result);

  std::string output = path.parent_path().string() + "/" + path.stem().string() + "_baked.gltf";
  spdlog::info("Writing to {}", output);
  tinygltf::TinyGLTF loader;
  loader.SetImagesAsIs(true);
  loader.WriteGltfSceneToFile(&model, output, false, false, true, false);
}

std::optional<tinygltf::Model> Baker::loadModel(const std::filesystem::path& path) {
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string error;
  std::string warning;
  bool success = false;

  // Load the glTF model from the specified file
  success = loader.LoadASCIIFromFile(&model, &error, &warning, path.string());
  auto extension = path.extension();
  if (extension == ".gltf")
    success = loader.LoadASCIIFromFile(&model, &error, &warning, path.string());
  else if (extension == ".glb")
    success = loader.LoadBinaryFromFile(&model, &error, &warning, path.string());
  else
  {
    spdlog::error("glTF: Unknown glTF file extension: '{}'. Expected .gltf or .glb.", extension);
    return std::nullopt;
  }

  if (!warning.empty()) {
    spdlog::warn("Warning loading file {}: {}", path.string(), warning);
  }
  if (!success) {
    spdlog::error("glTF: Failed to load model!");
    if (!error.empty())
      spdlog::error("glTF: {}", error);
    return std::nullopt;
  }
  if (
      !model.extensions.empty() || !model.extensionsRequired.empty() || !model.extensionsUsed.empty())
    spdlog::warn("glTF: No glTF extensions are currently implemented!");

  spdlog::info("Successfully loaded glTF file: {}", path.string());

  return model;
}


ProcessedMeshes Baker::processMeshes(const tinygltf::Model& model) {
  ProcessedMeshes result;
  // Pre-allocate enough memory so as not to hit the
  // allocator on the memcpy hotpath
  {
    std::size_t vertexBytes = 0;
    std::size_t indexBytes = 0;
    for (const auto& bufView : model.bufferViews)
    {
      switch (bufView.target)
      {
        case TINYGLTF_TARGET_ARRAY_BUFFER:
          vertexBytes += bufView.byteLength;
          break;
        case TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER:
          indexBytes += bufView.byteLength;
          break;
        default:
          break;
      }
    }
    spdlog::info("verticies {}", vertexBytes);
    result.vertices.reserve(vertexBytes / sizeof(Vertex));
    result.indices.reserve(indexBytes / sizeof(std::uint32_t));
  }

  {
    std::size_t totalPrimitives = 0;
    for (const auto& mesh : model.meshes)
      totalPrimitives += mesh.primitives.size();
    result.relems.reserve(totalPrimitives);
  }

  result.meshes.reserve(model.meshes.size());

  for (const auto& mesh : model.meshes)
  {
    result.meshes.push_back(Mesh{
        .firstRelem = static_cast<std::uint32_t>(result.relems.size()),
        .relemCount = static_cast<std::uint32_t>(mesh.primitives.size()),
        });

    for (const auto& prim : mesh.primitives)
    {
      if (prim.mode != TINYGLTF_MODE_TRIANGLES)
      {
        spdlog::warn(
            "Encountered a non-triangles primitive, these are not supported for now, skipping it!");
        --result.meshes.back().relemCount;
        continue;
      }

      const auto normalIt = prim.attributes.find("NORMAL");
      const auto tangentIt = prim.attributes.find("TANGENT");
      const auto texcoordIt = prim.attributes.find("TEXCOORD_0");

      const bool hasNormals = normalIt != prim.attributes.end();
      const bool hasTangents = tangentIt != prim.attributes.end();
      const bool hasTexcoord = texcoordIt != prim.attributes.end();
      std::array accessorIndices{
        prim.indices,
          prim.attributes.at("POSITION"),
          hasNormals ? normalIt->second : -1,
          hasTangents ? tangentIt->second : -1,
          hasTexcoord ? texcoordIt->second : -1,
      };

      std::array accessors{
        &model.accessors[prim.indices],
        &model.accessors[accessorIndices[1]],
        hasNormals ? &model.accessors[accessorIndices[2]] : nullptr,
        hasTangents ? &model.accessors[accessorIndices[3]] : nullptr,
        hasTexcoord ? &model.accessors[accessorIndices[4]] : nullptr,
      };

      std::array bufViews{
        &model.bufferViews[accessors[0]->bufferView],
        &model.bufferViews[accessors[1]->bufferView],
        hasNormals ? &model.bufferViews[accessors[2]->bufferView] : nullptr,
        hasTangents ? &model.bufferViews[accessors[3]->bufferView] : nullptr,
        hasTexcoord ? &model.bufferViews[accessors[4]->bufferView] : nullptr,
      };

      result.relems.push_back(RenderElement{
          .vertexOffset = static_cast<std::uint32_t>(result.vertices.size()),
          .indexOffset = static_cast<std::uint32_t>(result.indices.size()),
          .indexCount = static_cast<std::uint32_t>(accessors[0]->count),
          });

      const std::size_t vertexCount = accessors[1]->count;

      std::array ptrs{
        reinterpret_cast<const std::byte*>(model.buffers[bufViews[0]->buffer].data.data()) +
          bufViews[0]->byteOffset + accessors[0]->byteOffset,
          reinterpret_cast<const std::byte*>(model.buffers[bufViews[1]->buffer].data.data()) +
            bufViews[1]->byteOffset + accessors[1]->byteOffset,
          hasNormals
            ? reinterpret_cast<const std::byte*>(model.buffers[bufViews[2]->buffer].data.data()) +
            bufViews[2]->byteOffset + accessors[2]->byteOffset
            : nullptr,
          hasTangents
            ? reinterpret_cast<const std::byte*>(model.buffers[bufViews[3]->buffer].data.data()) +
            bufViews[3]->byteOffset + accessors[3]->byteOffset
            : nullptr,
          hasTexcoord
            ? reinterpret_cast<const std::byte*>(model.buffers[bufViews[4]->buffer].data.data()) +
            bufViews[4]->byteOffset + accessors[4]->byteOffset
            : nullptr,
      };

      std::array strides{
        bufViews[0]->byteStride != 0
          ? bufViews[0]->byteStride
          : tinygltf::GetComponentSizeInBytes(accessors[0]->componentType) *
          tinygltf::GetNumComponentsInType(accessors[0]->type),
          bufViews[1]->byteStride != 0
            ? bufViews[1]->byteStride
            : tinygltf::GetComponentSizeInBytes(accessors[1]->componentType) *
            tinygltf::GetNumComponentsInType(accessors[1]->type),
          hasNormals ? (bufViews[2]->byteStride != 0
              ? bufViews[2]->byteStride
              : tinygltf::GetComponentSizeInBytes(accessors[2]->componentType) *
              tinygltf::GetNumComponentsInType(accessors[2]->type))
            : 0,
          hasTangents ? (bufViews[3]->byteStride != 0
              ? bufViews[3]->byteStride
              : tinygltf::GetComponentSizeInBytes(accessors[3]->componentType) *
              tinygltf::GetNumComponentsInType(accessors[3]->type))
            : 0,
          hasTexcoord ? (bufViews[4]->byteStride != 0
              ? bufViews[4]->byteStride
              : tinygltf::GetComponentSizeInBytes(accessors[4]->componentType) *
              tinygltf::GetNumComponentsInType(accessors[4]->type))
            : 0,
      };

      for (std::size_t i = 0; i < vertexCount; ++i)
      {
        auto& vtx = result.vertices.emplace_back();
        glm::vec3 pos;
        // Fall back to 0 in case we don't have something.
        // NOTE: if tangents are not available, one could use http://mikktspace.com/
        // NOTE: if normals are not available, reconstructing them is possible but will look ugly
        glm::vec3 normal{0};
        glm::vec3 tangent{0};
        glm::vec2 texcoord{0};
        std::memcpy(&pos, ptrs[1], sizeof(pos));

        // NOTE: it's faster to do a template here with specializations for all combinations than to
        // do ifs at runtime. Also, SIMD should be used. Try implementing this!
        if (hasNormals)
          std::memcpy(&normal, ptrs[2], sizeof(normal));
        if (hasTangents)
          std::memcpy(&tangent, ptrs[3], sizeof(tangent));
        if (hasTexcoord)
          std::memcpy(&texcoord, ptrs[4], sizeof(texcoord));


        vtx.positionAndNormal = glm::vec4(pos, std::bit_cast<float>(encode_normal(glm::vec4(normal, 0))));
        vtx.texCoordAndTangentAndPadding =
          glm::vec4(texcoord, std::bit_cast<float>(encode_normal(glm::vec4(tangent, 1))), 0);

        ptrs[1] += strides[1];
        if (hasNormals)
          ptrs[2] += strides[2];
        if (hasTangents)
          ptrs[3] += strides[3];
        if (hasTexcoord)
          ptrs[4] += strides[4];
      }

      // Indices are guaranteed to have no stride
      ETNA_VERIFY(bufViews[0]->byteStride == 0);
      const std::size_t indexCount = accessors[0]->count;
      if (accessors[0]->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
      {
        for (std::size_t i = 0; i < indexCount; ++i)
        {
          std::uint16_t index;
          std::memcpy(&index, ptrs[0], sizeof(index));
          result.indices.push_back(index);
          ptrs[0] += 2;
        }
      }
      else if (accessors[0]->componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
      {
        const std::size_t lastTotalIndices = result.indices.size();
        result.indices.resize(lastTotalIndices + indexCount);
        std::memcpy(
            result.indices.data() + lastTotalIndices,
            ptrs[0],
            sizeof(result.indices[0]) * indexCount);
      }
    }
  }

  return result;
}


void Baker::updateBuffer(tinygltf::Model& model, ProcessedMeshes meshes, std::filesystem::path& path) {
  spdlog::info("Updating buffer");
  tinygltf::Buffer buffer;
  int indicesSize = meshes.indices.size() * sizeof(uint32_t);
  int verticesSize = meshes.vertices.size() * sizeof(Vertex);

  buffer.name = path.stem().string();
  buffer.uri = buffer.name + "_baked.bin";
  buffer.data.resize(indicesSize + verticesSize);

  memcpy(buffer.data.data(), meshes.indices.data(), indicesSize);
  memcpy(indicesSize + buffer.data.data(), meshes.vertices.data(), verticesSize);

  model.buffers.clear();
  model.buffers.push_back(buffer);
}

void Baker::updateBufferViews(tinygltf::Model& model, ProcessedMeshes meshes) {
  spdlog::info("Updating buffer views");
  int indicesSize = meshes.indices.size() * sizeof(uint32_t);
  int verticesSize = meshes.vertices.size() * sizeof(Vertex);

  tinygltf::BufferView indexView;
  indexView.name = "index_view",
  indexView.buffer = 0;
  indexView.byteOffset = 0;
  indexView.byteLength = indicesSize;
  indexView.byteStride = 0;
  indexView.target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;

  tinygltf::BufferView vertexView;
  vertexView.name = "vertex_view",
  vertexView.buffer = 0;
  vertexView.byteOffset = indexView.byteLength;
  vertexView.byteLength = verticesSize;
  vertexView.byteStride = sizeof(Vertex);
  vertexView.target = TINYGLTF_TARGET_ARRAY_BUFFER;

  model.bufferViews.clear();
  model.bufferViews.push_back(indexView);
  model.bufferViews.push_back(vertexView);
}

void Baker::updateAccessors(tinygltf::Model& model, ProcessedMeshes meshes) {
  spdlog::info("Updating accessors");

  tinygltf::Accessor indexAccessor;
  indexAccessor.bufferView = 0;
  indexAccessor.byteOffset = 0;
  indexAccessor.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
  indexAccessor.type = TINYGLTF_TYPE_SCALAR;
  indexAccessor.normalized = false;

  tinygltf::Accessor positionAccessor;
  positionAccessor.bufferView = 1;
  positionAccessor.byteOffset = 0;
  positionAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
  positionAccessor.type = TINYGLTF_TYPE_VEC3;
  positionAccessor.normalized = false;

  tinygltf::Accessor normalAccessor;
  normalAccessor.bufferView = 1;
  normalAccessor.byteOffset = 12;
  normalAccessor.componentType = TINYGLTF_COMPONENT_TYPE_BYTE;
  normalAccessor.type = TINYGLTF_TYPE_VEC3;
  normalAccessor.normalized = true;

  tinygltf::Accessor texcoordAccessor;
  texcoordAccessor.bufferView = 1;
  texcoordAccessor.byteOffset = 16;
  texcoordAccessor.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
  texcoordAccessor.type = TINYGLTF_TYPE_VEC2;
  texcoordAccessor.normalized = false;

  tinygltf::Accessor tangentAccessor;
  tangentAccessor.bufferView = 1;
  tangentAccessor.byteOffset = 24;
  tangentAccessor.componentType = TINYGLTF_COMPONENT_TYPE_BYTE;
  tangentAccessor.type = TINYGLTF_TYPE_VEC4;
  tangentAccessor.normalized = true;

  model.accessors.clear();

  for (size_t i = 0; i < model.meshes.size(); i++) {
      auto& mesh = model.meshes[i];

      for (size_t j = 0; j < mesh.primitives.size(); j++) {
        auto& prim = mesh.primitives[j];
        auto& relem = meshes.relems[meshes.meshes[i].firstRelem + j];

        const auto normalIt = prim.attributes.find("NORMAL");
        const auto texcoordIt = prim.attributes.find("TEXCOORD_0");
        const auto tangentIt = prim.attributes.find("TANGENT");

        const bool hasNormals = normalIt != prim.attributes.end();
        const bool hasTexcoord = texcoordIt != prim.attributes.end();
        const bool hasTangents = tangentIt != prim.attributes.end();

        {
          prim.indices = static_cast<int>(model.accessors.size());
          auto& accessor = model.accessors.emplace_back(indexAccessor);
          accessor.byteOffset += relem.indexOffset * sizeof(uint32_t);
          accessor.count = relem.indexCount;
        }

        if (hasNormals) {
          prim.attributes["POSITION"] = static_cast<int>(model.accessors.size());
          auto& accessor = model.accessors.emplace_back(positionAccessor);
          accessor.byteOffset += relem.vertexOffset * sizeof(Vertex);
          accessor.count = relem.indexCount;
        }

        if (hasNormals) {
          prim.attributes["NORMAL"] = static_cast<int>(model.accessors.size());
          auto& accessor = model.accessors.emplace_back(normalAccessor);
          accessor.byteOffset += relem.vertexOffset * sizeof(Vertex);
          accessor.count = relem.indexCount;
        }

        if (hasTexcoord) {
          prim.attributes["TEXCOORD_0"] = static_cast<int>(model.accessors.size());
          auto& accessor = model.accessors.emplace_back(texcoordAccessor);
          accessor.byteOffset += relem.vertexOffset * sizeof(Vertex);
          accessor.count = relem.indexCount;
        }

        if (hasTangents) {
          prim.attributes["TANGENT"] = static_cast<int>(model.accessors.size());
          auto& accessor = model.accessors.emplace_back(tangentAccessor);
          accessor.byteOffset += relem.vertexOffset * sizeof(Vertex);
          accessor.count = relem.indexCount;
        }
      }
    }
}


