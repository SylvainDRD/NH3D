cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
cmake_policy(SET CMP0043 NEW)

set(BINARY NH3D)
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 0)
set(PROJECT_VERSION_PATCH 1)
project(${BINARY} VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH} DESCRIPTION "")

add_definitions(-DPROJECT_NAME="${BINARY}" 
				-DPROJECT_VERSION_MAJOR=${PROJECT_VERSION_MAJOR} 
				-DPROJECT_VERSION_MINOR=${PROJECT_VERSION_MINOR}
				-DPROJECT_VERSION_PATCH=${PROJECT_VERSION_PATCH}
				-DPROJECT_DIR="${CMAKE_SOURCE_DIR}/")

file(GLOB_RECURSE SRC 	${CMAKE_SOURCE_DIR}/src/*.cpp
						${CMAKE_SOURCE_DIR}/src/scene/*.cpp
						${CMAKE_SOURCE_DIR}/src/scene/ecs/*.cpp
						${CMAKE_SOURCE_DIR}/src/scene/ecs/components/*.cpp
						${CMAKE_SOURCE_DIR}/src/general/*.cpp
						${CMAKE_SOURCE_DIR}/src/utils/*.cpp
						${CMAKE_SOURCE_DIR}/src/rendering/*.cpp
						${CMAKE_SOURCE_DIR}/src/rendering/render_graph/*.cpp
						${CMAKE_SOURCE_DIR}/src/rendering/vulkan/*.cpp
					)

add_executable(${BINARY} ${SRC})

set(NH3D_CXX_STANDARD cxx_std_20)
target_compile_features(${BINARY} PRIVATE ${NH3D_CXX_STANDARD})

if(NOT CMAKE_BUILD_TYPE MATCHES ".*Release.*")
	add_definitions(-DNH3D_DEBUG)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(${BINARY} PRIVATE -Wswitch)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	target_compile_options(${BINARY} PRIVATE /We4062)
endif()

# External dependencies
find_package(Vulkan REQUIRED)

set(DEPENDENCIES_PATH ${CMAKE_SOURCE_DIR}/external)

add_subdirectory(${DEPENDENCIES_PATH}/glm)
add_subdirectory(${DEPENDENCIES_PATH}/glfw)
add_subdirectory(${DEPENDENCIES_PATH}/VulkanMemoryAllocator)
add_subdirectory(${DEPENDENCIES_PATH}/json)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF CACHE INTERNAL "Disable TinyGLTF sample loader build" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "Disable TinyGLTF install" FORCE)
set(TINYGLTF_INSTALL_VENDOR OFF CACHE INTERNAL "Disable TinyGLTF dependency install" FORCE)
add_subdirectory(${DEPENDENCIES_PATH}/tinygltf)

set(NH3D_INCLUDES 	${CMAKE_SOURCE_DIR}/src
					${Vulkan_INCLUDE_DIRS}
					${DEPENDENCIES_PATH}/glm
					${DEPENDENCIES_PATH}/glfw/include
					${DEPENDENCIES_PATH}/VulkanMemoryAllocator/include
					${DEPENDENCIES_PATH}/json/include
					${DEPENDENCIES_PATH}/tinygltf
)
target_include_directories(${BINARY} PRIVATE ${NH3D_INCLUDES})

set(NH3D_LIBRARIES  ${Vulkan_LIBRARIES}
					glm
					glfw
					GPUOpen::VulkanMemoryAllocator

)
target_link_libraries(${BINARY} PRIVATE ${NH3D_LIBRARIES})

set(NH3D_DEPENDENCIES glfw)
add_dependencies(${BINARY} ${NH3D_DEPENDENCIES})

if(TRUE) 
	enable_testing() # Needs to be in at the top level so ctest doesn't shit itself
	add_subdirectory(${CMAKE_SOURCE_DIR}/tests)
endif()