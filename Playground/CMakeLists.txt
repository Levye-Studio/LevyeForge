add_executable(Playground main.cpp)
target_link_libraries(Playground PRIVATE LevyeForge)
# Copy only the resources this demo and the renderer actually require.
set(PLAYGROUND_RESOURCES "${CMAKE_SOURCE_DIR}/Editor/Resources")
add_custom_target(PlaygroundResources ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/Resources/Animations"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/Resources/Scenes"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PLAYGROUND_RESOURCES}/Scenes/PowerTheBeacon.UE" "${CMAKE_BINARY_DIR}/bin/Resources/Scenes/PowerTheBeacon.UE"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${PLAYGROUND_RESOURCES}/b992440bb3e22e2bcd0c1f236bf1afcf" "${CMAKE_BINARY_DIR}/bin/Resources/b992440bb3e22e2bcd0c1f236bf1afcf"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PLAYGROUND_RESOURCES}/cube.fbx" "${CMAKE_BINARY_DIR}/bin/Resources/cube.fbx"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${PLAYGROUND_RESOURCES}/Animations/Idle.fbx"
    "${PLAYGROUND_RESOURCES}/Animations/Running.fbx"
    "${PLAYGROUND_RESOURCES}/Animations/Jump.fbx"
    "${CMAKE_BINARY_DIR}/bin/Resources/Animations")
add_dependencies(Playground PlaygroundResources)
