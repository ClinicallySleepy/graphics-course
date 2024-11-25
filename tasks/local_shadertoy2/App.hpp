#pragma once

#include <etna/Sampler.hpp>
#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/ComputePipeline.hpp>
#include <etna/Image.hpp>

#include "etna/GraphicsPipeline.hpp"
#include "wsi/OsWindowingManager.hpp"

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
  void addMipLevels(etna::Image& image, vk::CommandBuffer& commandBuffer, size_t mipLevels, int width, int height, uint32_t layerCount = 1);

private:
  OsWindowingManager windowing;
  std::unique_ptr<OsWindow> osWindow;

  glm::uvec2 resolution;
  bool useVsync;

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
