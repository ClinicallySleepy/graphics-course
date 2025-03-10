#include "App.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <etna/Etna.hpp>
#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/RenderTargetStates.hpp>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_format_traits.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>
#include "etna/Image.hpp"
#include "spdlog/spdlog.h"
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct PushConstants {
  glm::vec2 resolution;
  glm::vec2 mouse;
  float time;
} pushConstants;

struct PushConstantsProcedural {
  glm::vec2 resolution;
  float time;
} pushConstantsProcedural;

App::App()
  : resolution{1280, 720}
  , useVsync{true}
{
  // First, we need to initialize Vulkan, which is not trivial because
  // extensions are required for just about anything.
  {
    // GLFW tells us which extensions it needs to present frames to the OS window.
    // Actually rendering anything to a screen is optional in Vulkan, you can
    // alternatively save rendered frames into files, send them over network, etc.
    // Instance extensions do not depend on the actual GPU, only on the OS.
    auto glfwInstExts = windowing.getRequiredVulkanInstanceExtensions();

    std::vector<const char*> instanceExtensions{glfwInstExts.begin(), glfwInstExts.end()};

    // We also need the swapchain device extension to get access to the OS
    // window from inside of Vulkan on the GPU.
    // Device extensions require HW support from the GPU.
    // Generally, in Vulkan, we call the GPU a "device" and the CPU/OS combination a "host."
    std::vector<const char*> deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    // Etna does all of the Vulkan initialization heavy lifting.
    // You can skip figuring out how it works for now.
    etna::initialize(etna::InitParams{
      .applicationName = "InflightFrames",
      .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
      .instanceExtensions = instanceExtensions,
      .deviceExtensions = deviceExtensions,
      // Replace with an index if etna detects your preferred GPU incorrectly
      // .physicalDeviceIndexOverride = 1,
      .numFramesInFlight = 1,
    });
  }

  // Now we can create an OS window
  osWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = resolution,
  });

  // But we also need to hook the OS window up to Vulkan manually!
  {
    // First, we ask GLFW to provide a "surface" for the window,
    // which is an opaque description of the area where we can actually render.
    auto surface = osWindow->createVkSurface(etna::get_context().getInstance());

    // Then we pass it to Etna to do the complicated work for us
    vkWindow = etna::get_context().createWindow(etna::Window::CreateInfo{
      .surface = std::move(surface),
    });

    // And finally ask Etna to create the actual swapchain so that we can
    // get (different) images each frame to render stuff into.
    // Here, we do not support window resizing, so we only need to call this once.
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });

    // Technically, Vulkan might fail to initialize a swapchain with the requested
    // resolution and pick a different one. This, however, does not occur on platforms
    // we support. Still, it's better to follow the "intended" path.
    resolution = {w, h};
  }

  // Next, we need a magical Etna helper to send commands to the GPU.
  // How it is actually performed is not trivial, but we can skip this for now.
  commandManager = etna::get_context().createPerFrameCmdMgr();

  etna::create_program(
      "toy_fragment",
      { INFLIGHT_FRAMES_SHADERS_ROOT "toy.frag.spv", INFLIGHT_FRAMES_SHADERS_ROOT "toy.vert.spv" });

  etna::create_program(
      "toy_procedural",
      { INFLIGHT_FRAMES_SHADERS_ROOT "procedural.frag.spv", INFLIGHT_FRAMES_SHADERS_ROOT "toy.vert.spv" });

  graphicsPipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
      "toy_fragment", 
      etna::GraphicsPipeline::CreateInfo{
        .fragmentShaderOutput =
          {
            .colorAttachmentFormats = {
            vkWindow->getCurrentFormat(),
            },
          },
    });

  proceduralPipeline = etna::get_context().getPipelineManager().createGraphicsPipeline(
      "toy_procedural", 
      etna::GraphicsPipeline::CreateInfo{
        .fragmentShaderOutput =
          {
            .colorAttachmentFormats = {vkWindow->getCurrentFormat()},
          },
    });

  proceduralImage = etna::get_context().createImage(etna::Image::CreateInfo{
    .extent = vk::Extent3D{resolution.x, resolution.y, 1},
    .name = "result_image",
    .format = vkWindow->getCurrentFormat(),
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
  });

  defaultSampler = etna::Sampler(etna::Sampler::CreateInfo{.name = "default_sampler"});
  checkerSampler = etna::Sampler(etna::Sampler::CreateInfo{
    .addressMode = vk::SamplerAddressMode::eRepeat,
    .name = "default_sampler",
  });
  skyboxSampler = etna::Sampler(etna::Sampler::CreateInfo{
    .filter = vk::Filter::eLinear,
    .name = "skybox_sampler",
  });

  createCheckerImage();
  createSkyboxImage();
}

App::~App()
{
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

void App::run()
{
  while (!osWindow->isBeingClosed())
  {
    windowing.poll();

    drawFrame();
  }

  // We need to wait for the GPU to execute the last frame before destroying
  // all resources and closing the application.
  ETNA_CHECK_VK_RESULT(etna::get_context().getDevice().waitIdle());
}

// wasnt able to make this work
void App::addMipLevels(etna::Image& image, vk::CommandBuffer& command_buffer, size_t mip_levels, int width, int height, uint32_t layer_count) {
  if (mip_levels <= 1) {
    spdlog::error("incorrect mip levels: {}", mip_levels);
    return;
  }
  else {
    spdlog::info("mip levels: {}", mip_levels);
  };

  ETNA_CHECK_VK_RESULT(command_buffer.begin(vk::CommandBufferBeginInfo{
    .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  }));


  for (uint32_t i = 1; i < mip_levels; ++i)
  {
    // destination mip level setup
    vk::ImageMemoryBarrier2 transitionBarrier{
      .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
      .srcAccessMask = vk::AccessFlagBits2::eTransferRead,
      .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
      .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
      .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
      .newLayout = vk::ImageLayout::eTransferDstOptimal,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image.get(),
      .subresourceRange = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = i,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = layer_count
      }
    };
    vk::DependencyInfo depInfo{
      .dependencyFlags = vk::DependencyFlagBits::eByRegion,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &transitionBarrier
    };
    command_buffer.pipelineBarrier2(depInfo);

    vk::ImageBlit imageBlitRegion{};

    imageBlitRegion.srcSubresource = VkImageSubresourceLayers{VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, layer_count};
    imageBlitRegion.srcOffsets[1] = VkOffset3D{std::max(int(width >> (i - 1)), 1), std::max(int(height >> (i - 1)), 1), 1};
    imageBlitRegion.dstSubresource = VkImageSubresourceLayers{VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT, i, 0, layer_count};
    imageBlitRegion.dstOffsets[1] = VkOffset3D{std::max(int(width >> i), 1), std::max(int(height >> i), 1), 1};

    command_buffer.blitImage(
        image.get(),
        vk::ImageLayout::eTransferSrcOptimal,
        image.get(),
        vk::ImageLayout::eTransferDstOptimal,
        1,
        &imageBlitRegion,
        vk::Filter::eLinear
        );
  }

  // return to the original (after image creation) barrier
  for (uint32_t i = 1; i < mip_levels; i++)
  {
    vk::ImageMemoryBarrier2 transitionBarrier{
      .srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
      .srcAccessMask = vk::AccessFlagBits2::eTransferRead,
      .dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
      .dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
      .oldLayout = vk::ImageLayout::eTransferSrcOptimal,
      .newLayout = vk::ImageLayout::eTransferDstOptimal,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image.get(),
      .subresourceRange = {
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = i,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = layer_count
      }
    };
    vk::DependencyInfo depInfo{
      .dependencyFlags = vk::DependencyFlagBits::eByRegion,
      .imageMemoryBarrierCount = 1,
      .pImageMemoryBarriers = &transitionBarrier
    };
    command_buffer.pipelineBarrier2(depInfo);
    etna::flush_barriers(command_buffer);
  }
  
  ETNA_CHECK_VK_RESULT(command_buffer.end());

  vk::SubmitInfo submitInfo{
    .commandBufferCount = 1,
      .pCommandBuffers = &command_buffer,
  };

  ETNA_CHECK_VK_RESULT(etna::get_context().getQueue().submit(1, &submitInfo, {}));
  ETNA_CHECK_VK_RESULT(etna::get_context().getQueue().waitIdle());
}

void App::createCheckerImage() {
  auto commandBuffer = commandManager->acquireNext();
  int width, height;
  auto imageData = stbi_load("../resources/textures/shadertoy_checker.png", &width, &height, nullptr, 4);

  ETNA_VERIFY(imageData);

  size_t mipLevels = static_cast<size_t>(floor(log2(std::max(width, height))) + 1);
  auto imageInfo = etna::Image::CreateInfo{
    .extent = vk::Extent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
    .name = "checker_image",
    .format = vkWindow->getCurrentFormat(),
    .imageUsage = vk::ImageUsageFlagBits::eSampled,
    .layers = 1,
    .mipLevels = mipLevels,
  };
  checkerImage = etna::create_image_from_bytes(imageInfo, commandBuffer, imageData);
  // addMipLevels(checkerImage, commandBuffer, mipLevels, width, height);
  stbi_image_free(imageData);
}

void App::createSkyboxImage() {
  auto commandBuffer = commandManager->acquireNext();
  int width, height;
  stbi_uc* imagesData[6];

  for (int i = 0; i < 6; ++i) {
    std::string filepath = std::format("../resources/textures/shadertoy_skybox{}.jpg", i);

    imagesData[i] = stbi_load(filepath.c_str(), &width, &height, nullptr, 4);
    ETNA_VERIFY(imagesData[i]);
  }

  size_t mipLevels = static_cast<size_t>(floor(log2(std::max(width, height))) + 1);
  auto imageInfo = etna::Image::CreateInfo{
    .extent = vk::Extent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
    .name = "cubemap_image",
    .format = vkWindow->getCurrentFormat(),
    .imageUsage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc,
    .layers = 6,
    .mipLevels = mipLevels,
    .flags = vk::ImageCreateFlagBits::eCubeCompatible,
  };
  const auto blockSize = vk::blockSize(imageInfo.format);
  const auto layerSize = blockSize * imageInfo.extent.width * imageInfo.extent.height * imageInfo.extent.depth;
  const auto imageSize = layerSize * imageInfo.layers;
  etna::Buffer stagingBuf = etna::get_context().createBuffer(etna::Buffer::CreateInfo{
    .size = imageSize,
    .bufferUsage = vk::BufferUsageFlagBits::eTransferSrc,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "temporary_buffer",
  });

  auto* mappedMem = stagingBuf.map();
  for (int i = 0; i < 6; ++i) {
    std::memcpy(mappedMem + (layerSize * i), imagesData[i], layerSize);
    stbi_image_free(imagesData[i]);
  }
  stagingBuf.unmap();

  ETNA_CHECK_VK_RESULT(commandBuffer.begin(vk::CommandBufferBeginInfo{
    .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  }));

  skyboxImage = etna::get_context().createImage(imageInfo);
  etna::set_state(
      commandBuffer,
      skyboxImage.get(),
      vk::PipelineStageFlagBits2::eTransfer,
      vk::AccessFlagBits2::eTransferWrite,
      vk::ImageLayout::eTransferDstOptimal,
      skyboxImage.getAspectMaskByFormat());
  etna::flush_barriers(commandBuffer);

  vk::BufferImageCopy region{
    .bufferOffset = 0,
      .bufferRowLength = 0,
      .bufferImageHeight = 0,
      .imageSubresource =
        vk::ImageSubresourceLayers{
          .aspectMask = skyboxImage.getAspectMaskByFormat(),
          .mipLevel = 0,
          .baseArrayLayer = 0,
          .layerCount = static_cast<std::uint32_t>(imageInfo.layers),
        },
      .imageOffset = {0, 0, 0},
      .imageExtent = imageInfo.extent,
  };

  commandBuffer.copyBufferToImage(
      stagingBuf.get(), skyboxImage.get(), vk::ImageLayout::eTransferDstOptimal, 1, &region);

  ETNA_CHECK_VK_RESULT(commandBuffer.end());

  vk::SubmitInfo submitInfo{
    .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer,
  };

  ETNA_CHECK_VK_RESULT(etna::get_context().getQueue().submit(1, &submitInfo, {}));
  ETNA_CHECK_VK_RESULT(etna::get_context().getQueue().waitIdle());

  stagingBuf.reset();
  addMipLevels(skyboxImage, commandBuffer, mipLevels, width, height, imageInfo.layers);
}

void App::drawFrame()
{
  // First, get a command buffer to write GPU commands into.
  auto currentCmdBuf = commandManager->acquireNext();

  // Next, tell Etna that we are going to start processing the next frame.
  etna::begin_frame();

  // And now get the image we should be rendering the picture into.
  auto nextSwapchainImage = vkWindow->acquireNext();

  // When window is minimized, we can't render anything in Windows
  // because it kills the swapchain, so we skip frames in this case.
  if (nextSwapchainImage)
  {
    auto [backbuffer, backbufferView, backbufferAvailableSem] = *nextSwapchainImage;

    ETNA_CHECK_VK_RESULT(currentCmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      etna::set_state(
        currentCmdBuf,
        proceduralImage.get(),
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageAspectFlagBits::eColor);
      etna::flush_barriers(currentCmdBuf);

      {
        etna::RenderTargetState state{
          currentCmdBuf,
          vk::Rect2D{{0, 0}, { resolution.x, resolution.y }},
          {{proceduralImage.get(), proceduralImage.getView({})}},
          {}
        };
        currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, proceduralPipeline.getVkPipeline());
        pushConstantsProcedural.resolution = resolution;
        pushConstantsProcedural.time = windowing.getTime();

        currentCmdBuf.pushConstants(
          proceduralPipeline.getVkPipelineLayout(),
          vk::ShaderStageFlagBits::eFragment,
          0,
          sizeof(pushConstantsProcedural),
          &pushConstantsProcedural);

        currentCmdBuf.draw(3, 1, 0, 0);
      };

      {
        auto graphicsInfo = etna::get_shader_program("toy_fragment");

        auto set = etna::create_descriptor_set(
          graphicsInfo.getDescriptorLayoutId(0),
          currentCmdBuf,
          {
            etna::Binding{0, proceduralImage.genBinding(defaultSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
            etna::Binding{1, checkerImage.genBinding(checkerSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)},
            etna::Binding{2, skyboxImage.genBinding(skyboxSampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal, etna::Image::ViewParams({
              .baseMip = 1,
              .type = vk::ImageViewType::eCube,
            }))},
          });

        etna::RenderTargetState state{currentCmdBuf,
          vk::Rect2D{{}, { resolution.x, resolution.y }},
          {
            {backbuffer, backbufferView},
          },
          {}
        };

        vk::DescriptorSet vkSet = set.getVkSet();

        currentCmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline.getVkPipeline());
        currentCmdBuf.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          graphicsPipeline.getVkPipelineLayout(),
          0,
          1,
          &vkSet,
          0,
          nullptr);

        pushConstants.resolution = resolution;
        pushConstants.mouse = osWindow->mouse.freePos;
        pushConstants.time = windowing.getTime();

        currentCmdBuf.pushConstants(
          graphicsPipeline.getVkPipelineLayout(),
          vk::ShaderStageFlagBits::eFragment,
          0,
          sizeof(pushConstants),
          &pushConstants);

        currentCmdBuf.draw(3, 1, 0, 0);
      };
    }

    // And of course flush the layout transition.
    etna::flush_barriers(currentCmdBuf);

    ETNA_CHECK_VK_RESULT(currentCmdBuf.end());

    // We are done recording GPU commands now and we can send them to be executed by the GPU.
    // Note that the GPU won't start executing our commands before the semaphore is
    // signalled, which will happen when the OS says that the next swapchain image is ready.
    auto renderingDone =
      commandManager->submit(std::move(currentCmdBuf), std::move(backbufferAvailableSem));

    // Finally, present the backbuffer the screen, but only after the GPU tells the OS
    // that it is done executing the command buffer via the renderingDone semaphore.
    const bool presented = vkWindow->present(std::move(renderingDone), backbufferView);

    if (!presented)
      nextSwapchainImage = std::nullopt;
  }

  etna::end_frame();

  // After a window us un-minimized, we need to restore the swapchain to continue rendering.
  if (!nextSwapchainImage && osWindow->getResolution() != glm::uvec2{0, 0})
  {
    auto [w, h] = vkWindow->recreateSwapchain(etna::Window::DesiredProperties{
      .resolution = {resolution.x, resolution.y},
      .vsync = useVsync,
    });
    ETNA_VERIFY((resolution == glm::uvec2{w, h}));
  }
}
