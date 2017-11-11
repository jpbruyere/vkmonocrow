cmake_minimum_required(VERSION 3.7)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

project(libvk)

find_package(Vulkan REQUIRED)
find_package(GLFW3 REQUIRED)

set(CMAKE_EXE_LINKER_FLAGS "-lm")

add_executable(${PROJECT_NAME} "vke.c")
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARY} ${GLFW3_LIBRARY})
