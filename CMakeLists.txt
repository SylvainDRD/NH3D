cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
cmake_policy(SET CMP0043 NEW)

set(NH3D_LIB NH3D)
set(NH3D_VERSION_MAJOR 0)
set(NH3D_VERSION_MINOR 0)
set(NH3D_VERSION_PATCH 1)
project(${NH3D_LIB} VERSION ${NH3D_VERSION_MAJOR}.${NH3D_VERSION_MINOR}.${NH3D_VERSION_PATCH} DESCRIPTION "")

## Library
file(GLOB NH3D_SOURCES 	${CMAKE_SOURCE_DIR}/src/scene/*.cpp
                        ${CMAKE_SOURCE_DIR}/src/scene/ecs/*.cpp
                        ${CMAKE_SOURCE_DIR}/src/scene/ecs/components/*.cpp
                        ${CMAKE_SOURCE_DIR}/src/core/*.cpp
                        ${CMAKE_SOURCE_DIR}/src/general/*.cpp
                        ${CMAKE_SOURCE_DIR}/src/utils/*.cpp
                        ${CMAKE_SOURCE_DIR}/src/rendering/*.cpp
                        ${CMAKE_SOURCE_DIR}/src/rendering/render_graph/*.cpp
                        ${CMAKE_SOURCE_DIR}/src/rendering/vulkan/*.cpp
                        ${CMAKE_SOURCE_DIR}/external/imgui/*.cpp
)

add_library(${NH3D_LIB} STATIC ${NH3D_SOURCES})

target_compile_definitions(${NH3D_LIB} PRIVATE  -DNH3D_NAME="${NH3D_LIB}"
            -DNH3D_VERSION_MAJOR=${NH3D_VERSION_MAJOR}
            -DNH3D_VERSION_MINOR=${NH3D_VERSION_MINOR}
            -DNH3D_VERSION_PATCH=${NH3D_VERSION_PATCH}
            -DNH3D_DIR="${CMAKE_SOURCE_DIR}/"
)


set(NH3D_CXX_STANDARD cxx_std_20)
target_compile_features(${NH3D_LIB} PRIVATE ${NH3D_CXX_STANDARD})

if(NOT CMAKE_BUILD_TYPE MATCHES ".*Release.*")
    # Add custom debug defines for RelWithDebInfo mode which enables asserts
    target_compile_definitions(${NH3D_LIB} PRIVATE NH3D_DEBUG)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(FATAL_ERROR "The project is facing an internal compiler error when trying to build with MSVC, install Clang")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${NH3D_LIB} PRIVATE -Wswitch)
else()
    message(FATAL_ERROR "What kind of compiler is that?")
endif()

# External dependencies
find_package(Vulkan REQUIRED)

set(DEPENDENCIES_PATH ${CMAKE_SOURCE_DIR}/external)

add_subdirectory(${DEPENDENCIES_PATH}/glm)
add_subdirectory(${DEPENDENCIES_PATH}/VulkanMemoryAllocator)
add_subdirectory(${DEPENDENCIES_PATH}/json)
add_subdirectory(${DEPENDENCIES_PATH}/SDL EXCLUDE_FROM_ALL)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE INTERNAL "Disable TinyGLTF sample loader build" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "Disable TinyGLTF install" FORCE)
set(TINYGLTF_INSTALL_VENDOR OFF CACHE INTERNAL "Disable TinyGLTF dependency install" FORCE)
add_subdirectory(${DEPENDENCIES_PATH}/tinygltf)

set(NH3D_INCLUDES 	${CMAKE_SOURCE_DIR}/src
                    ${Vulkan_INCLUDE_DIRS}
                    ${DEPENDENCIES_PATH}/glm
                    ${DEPENDENCIES_PATH}/VulkanMemoryAllocator/include
                    ${DEPENDENCIES_PATH}/json/include
                    ${DEPENDENCIES_PATH}/tinygltf
                    ${DEPENDENCIES_PATH}/imgui
                    ${DEPENDENCIES_PATH}/SDL/include
)
target_include_directories(${NH3D_LIB} PRIVATE ${NH3D_INCLUDES})

set(NH3D_LIBRARIES  ${Vulkan_LIBRARIES}
                    glm
                    SDL3-static
                    GPUOpen::VulkanMemoryAllocator
)
target_link_libraries(${NH3D_LIB} PRIVATE ${NH3D_LIBRARIES})

## Test suite
if(TRUE)
    enable_testing() # Needs to be in at the top level so ctest doesn't shit itself
    add_subdirectory(${CMAKE_SOURCE_DIR}/tests)
endif()

## Editor
set(NH3D_EDITOR_BIN NH3D-Editor)
file(GLOB NH3D_EDITOR_SOURCES ${CMAKE_SOURCE_DIR}/src/editor/main.cpp 
                              ${CMAKE_SOURCE_DIR}/external/imgui/*.cpp
)
add_executable(${NH3D_EDITOR_BIN} ${NH3D_EDITOR_SOURCES})

target_compile_features(${NH3D_EDITOR_BIN} PRIVATE ${NH3D_CXX_STANDARD})

target_compile_definitions(${NH3D_EDITOR_BIN} PRIVATE   -DNH3D_NAME="${NH3D_LIB}"
                                                        -DNH3D_VERSION_MAJOR=${NH3D_VERSION_MAJOR}
                                                        -DNH3D_VERSION_MINOR=${NH3D_VERSION_MINOR}
                                                        -DNH3D_VERSION_PATCH=${NH3D_VERSION_PATCH}
                                                        -DNH3D_DIR="${CMAKE_SOURCE_DIR}/"
)

target_link_libraries(${NH3D_EDITOR_BIN} PRIVATE ${NH3D_LIB})

target_include_directories(${NH3D_EDITOR_BIN} PRIVATE ${NH3D_INCLUDES})
