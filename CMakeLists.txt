cmake_minimum_required(VERSION 3.7)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

project(libvk)


find_package(Vulkan REQUIRED)
find_package(GLFW3 REQUIRED)

set(CMAKE_EXE_LINKER_FLAGS "-lm")

# Find glslc shader compiler.
# On Android, the NDK includes the binary, so no external dependency.
if(ANDROID)
	file(GLOB glslc-folders ${ANDROID_NDK}/shader-tools/*)
else()
	file(GLOB glslc-folders ${VULKAN_SDK}/bin)
endif()
find_program(GLSLC glslc HINTS ${glslc-folders})

set(SHADER_DIR "shaders")
set(SHADER_FILES ${SHADER_DIR}/*.frag ${SHADER_DIR}/*.vert ${SHADER_DIR}/*.geom  ${SHADER_DIR}/*.comp)
file(GLOB_RECURSE SHADERS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${SHADER_FILES})
foreach(SHADER ${SHADERS})
	set(shader-input ${CMAKE_CURRENT_SOURCE_DIR}/${SHADER})
	set(shader-output ${CMAKE_CURRENT_BINARY_DIR}/${SHADER}.spv)
	add_custom_command (
	OUTPUT ${shader-output}
	  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_DIR}"
	  COMMAND ${GLSLC} ${shader-input} -o ${shader-output}
	  COMMENT "Compiling ${shader-input}"
	  DEPENDS ${SHADER}
	  VERBATIM
	)
    set(SHADER_OUTPUTS ${SHADER_OUTPUTS} ${shader-output})
endforeach()
add_custom_target(CompileShaders ALL DEPENDS ${SHADER_OUTPUTS})

find_package(PkgConfig)
pkg_check_modules (MONO2   mono-2)

include_directories(${MONO2_INCLUDE_DIRS})
link_directories(${MONO2_LIBRARY_DIRS})

add_executable(${PROJECT_NAME} "vke.c" "compute.c" "crow.c" ${SHADERS})
target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARY} ${GLFW3_LIBRARY} ${MONO2_LIBRARIES})
