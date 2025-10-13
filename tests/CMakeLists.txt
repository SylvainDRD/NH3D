set(gtest_force_shared_crt ON CACHE BOOL "For Windows: Prevent overriding the parent project's compiler/linker settings" FORCE)

set(INSTALL_GTEST OFF CACHE INTERNAL "Disable gtest install" FORCE)
set(BUILD_GMOCK OFF CACHE INTERNAL "Disable gmock build" FORCE)
add_subdirectory(${external}/googletest ${CMAKE_BINARY_DIR}/gtest)

include(GoogleTest)

# TODO: foreach, see GLM tests
if(${Vulkan_FOUND})
    set(VK_TEST enums)

    add_executable(${VK_TEST} rendering/vulkan/${VK_TEST}.cpp)

    target_include_directories(${VK_TEST} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_include_directories(${VK_TEST} PRIVATE ${Vulkan_INCLUDE_DIRS} ${external}/VulkanMemoryAllocator/include)
    target_link_libraries(${VK_TEST} PRIVATE GTest::gtest_main)
    gtest_discover_tests(${VK_TEST})
endif()
