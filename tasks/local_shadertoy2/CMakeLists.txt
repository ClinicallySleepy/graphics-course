
add_executable(local_shadertoy_fragment
  main.cpp
  App.cpp
)

target_link_libraries(local_shadertoy_fragment
  PRIVATE glfw etna glm::glm wsi gui)

target_add_shaders(local_shadertoy_fragment
  shaders/toy.vert
  shaders/toy.frag
  shaders/procedural.frag
)
