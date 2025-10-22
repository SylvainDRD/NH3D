#include "gtest/gtest.h"
#include <scene/ecs/sparse_set_map.hpp>
#include <scene/ecs/entity.hpp>
#include <gtest/gtest.h>
#include <string>

namespace NH3D::Test {

    struct A {
        uint32 test;
        bool testBool;
    };

    TEST(SparseSetMapTests, MaskTest) {
        SparseSetMap map;

        ComponentMask mask =  map.mask<int, char, bool, std::string, A>();
        EXPECT_EQ(mask, 31);

        mask = map.mask<int, bool>();
        EXPECT_EQ(mask, 5);

        mask = map.mask<char, std::string>();
        EXPECT_EQ(mask, 10);

        mask = map.mask<A>();
        EXPECT_EQ(mask, 16);
    }

    TEST(SparseSetMapTests, AddGetTest) {
        SparseSetMap map;

        const Entity e1 = 0;
        map.add(e1, 42, 'a', A{1337, true});

        const Entity e2 = 1;
        map.add(e2, 10, std::string{"Test str"}, A{20, true});
        
        EXPECT_EQ(map.get<char>(e1), 'a');
        EXPECT_EQ(map.get<A>(e1).test, 1337);
        EXPECT_EQ(map.get<int>(e1), 42);
        EXPECT_EQ(map.get<A>(e1).testBool, true);

        EXPECT_EQ(map.get<std::string>(e2), std::string{"Test str"});
        EXPECT_EQ(map.get<A>(e2).testBool, true);
        EXPECT_EQ(map.get<A>(e2).test, 20);
        EXPECT_EQ(map.get<int>(e2), 10);
    }

    TEST(SparseSetMapTests, RemoveMaskTest) {
        SparseSetMap map;

        const Entity e1 = 0;
        map.add(e1, 42, true, 'a', A{1337, true});
        EXPECT_DEATH(map.add(e1, 42, true, 'a', A{1337, true}), ".*FATAL.*Trying to overwrite an existing component");

        const Entity e2 = 1;
        map.add(e2, 10, std::string{"Test str"}, A{20, true});
        
        const ComponentMask mask = map.mask<int, bool, char, A>();
        map.remove(e1, mask);
        EXPECT_NO_FATAL_FAILURE(map.add(e1));

        EXPECT_DEATH(map.remove<int>(e1), ".*FATAL.*Trying to clear a non-existing component");
    }

}