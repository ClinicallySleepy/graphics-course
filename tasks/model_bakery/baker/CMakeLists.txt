
add_executable(model_bakery_baker
  main.cpp
  Baker.cpp
)

target_link_libraries(model_bakery_baker
  PRIVATE tinygltf etna glm::glm)

# target_link_libraries(inflight_frames
#   PRIVATE tinygltf glfw etna glm::glm wsi gui render_utils scene)
