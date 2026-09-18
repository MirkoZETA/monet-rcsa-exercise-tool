cmake_minimum_required(VERSION 3.20)

project(monet-exercise LANGUAGES CXX)

find_package(Threads REQUIRED)

add_executable(monet-exercise simulator/main.cpp)
target_compile_features(monet-exercise PRIVATE cxx_std_20)
target_link_libraries(monet-exercise PRIVATE Threads::Threads)

# Keep runtime resources beside the executable. The simulator resolves its
# bundled files from this location regardless of the current working directory.
add_custom_command(
  TARGET monet-exercise POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/simulator/resources/topologies"
    "$<TARGET_FILE_DIR:monet-exercise>/resources/topologies"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/simulator/resources/demands"
    "$<TARGET_FILE_DIR:monet-exercise>/resources/demands"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/simulator/resources/bitrates.json"
    "$<TARGET_FILE_DIR:monet-exercise>/resources/bitrates.json"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/simulator/resources/SSMF_C.json"
    "$<TARGET_FILE_DIR:monet-exercise>/resources/SSMF_C.json"
)

install(TARGETS monet-exercise RUNTIME DESTINATION bin)
install(DIRECTORY simulator/resources/ DESTINATION bin/resources)
