#pragma once

#include <etna/Sampler.hpp>
#include <etna/Window.hpp>
#include <etna/Buffer.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Image.hpp>

#include "etna/GraphicsPipeline.hpp"
#include "wsi/OsWindowingManager.hpp"

struct UniformParams {
  glm::vec2 resolution;
  glm::vec2 mouse;
  float time;
};


class App
{
public:
  App();
  ~App();

  void run();

private:
  void drawFrame();
  void createCheckerImage();
  void createSkyboxImage();
  void addMipLevels(etna::Image& image, vk::CommandBuffer& command_buffer, size_t mip_levels, int width, int height, uint32_t layer_count = 1);

private:
  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  int frameCounter = 0;
  glm::uvec2 resolution;
  bool useVsync;
  etna::Buffer constants[2];
  UniformParams uniformParams;

  std::unique_ptr<etna::Window> vkWindow;
  std::unique_ptr<etna::PerFrameCmdMgr> commandManager;
  etna::Image proceduralImage;
  etna::Image checkerImage;
  etna::Image skyboxImage;
  etna::Sampler defaultSampler;
  etna::Sampler checkerSampler;
  etna::Sampler skyboxSampler;
  // etna::ComputePipeline pipeline;
  etna::GraphicsPipeline graphicsPipeline;
  etna::GraphicsPipeline proceduralPipeline;
};
