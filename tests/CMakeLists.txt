set(gtest_force_shared_crt ON CACHE BOOL "For Windows: Prevent overriding the parent project's compiler/linker settings" FORCE)

set(INSTALL_GTEST OFF CACHE INTERNAL "Disable gtest install" FORCE)
set(BUILD_GMOCK OFF CACHE INTERNAL "Disable gmock build" FORCE)
add_subdirectory(${DEPENDENCIES_PATH}/googletest ${CMAKE_BINARY_DIR}/gtest)

include(GoogleTest)

function (declare_test path) 
    get_filename_component(TEST_NAME ${path} NAME_WE)
    add_executable(${TEST_NAME} ${path})

    target_compile_features(${TEST_NAME} PRIVATE ${NH3D_CXX_STANDARD})
    target_include_directories(${TEST_NAME} PRIVATE ${NH3D_INCLUDES})
    target_link_libraries(${TEST_NAME} PRIVATE ${NH3D_LIBRARIES} GTest::gtest_main)
    # Force assertions enabled even in release mode
	target_compile_definitions(${TEST_NAME} PRIVATE NH3D_DEBUG) 
    add_dependencies(${TEST_NAME} ${NH3D_DEPENDENCIES})
    gtest_discover_tests(${TEST_NAME})
endfunction()

if(${Vulkan_FOUND})
    declare_test(rendering/vulkan/enums.cpp)
    declare_test(rendering/core/resource_manager.cpp)
    declare_test(rendering/core/split_pool.cpp)
    declare_test(scene/ecs/component_view.cpp)
    declare_test(scene/ecs/entity_manager.cpp)
    declare_test(scene/ecs/sparse_set_map.cpp)
    declare_test(scene/ecs/sparse_set.cpp)
    declare_test(scene/hierarchy.cpp)
endif()
