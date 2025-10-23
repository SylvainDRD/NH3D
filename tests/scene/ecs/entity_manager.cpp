#include "scene/ecs/entity.hpp"
#include "gtest/gtest.h"
#include <gtest/gtest.h>
#include <scene/ecs/entity_manager.hpp>

namespace NH3D::Test {

TEST(EntityManagerTests, CreateTest)
{
    EXPECT_DEATH((void)EntityManager::get<int>(0), ".*FATAL.*Requested a component for an entity without storage");

    const Entity e = EntityManager::create(3, 'a');
    EXPECT_EQ(EntityManager::get<int>(e), 3);
    EXPECT_EQ(EntityManager::get<char>(e), 'a');

    EXPECT_DEATH((void)EntityManager::get<int>(e + 1), ".*FATAL.*Requested a non-existing component: Samir you're thrashing the cache");

    EntityManager::remove(e);

    EXPECT_EQ(EntityManager::create(1), e);
}

TEST(EntityManagerTests, AddTest)
{
    const Entity e1 = EntityManager::create('a');
    const Entity e2 = EntityManager::create('b');

    EntityManager::add(e1, 1337);

    EXPECT_EQ(EntityManager::get<int>(e1), 1337);
    EXPECT_EQ(EntityManager::get<char>(e1), 'a');
    EXPECT_EQ(EntityManager::get<char>(e2), 'b');
}

TEST(EntityManagerTests, HasTest)
{
    const Entity e1 = EntityManager::create('a', 2);
    const Entity e2 = EntityManager::create('b', true);

    EXPECT_DEATH((void)EntityManager::has<int>(3), ".*FATAL.*Trying to check ComponentMask for a non-existing entity");

    EXPECT_TRUE(EntityManager::has<int>(e1));
    EXPECT_TRUE(EntityManager::has<char>(e1));
    EXPECT_FALSE(EntityManager::has<bool>(e1));
    EXPECT_TRUE(EntityManager::has<char>(e2));
    EXPECT_TRUE(EntityManager::has<bool>(e2));
    EXPECT_FALSE(EntityManager::has<int>(e2));

    bool has = EntityManager::has<int, char>(e1);
    EXPECT_TRUE(has);
    has = EntityManager::has<int, bool>(e1);
    EXPECT_FALSE(has);
    has = EntityManager::has<bool, char>(e2);
    EXPECT_TRUE(has);
    has = EntityManager::has<int, char>(e2);
    EXPECT_FALSE(has);
}

TEST(EntityManagerTests, ClearComponentTest)
{
    const Entity e = EntityManager::create('a', 2);

    EntityManager::clearComponent<char>(e);
    EXPECT_FALSE(EntityManager::has<char>(e));
    EXPECT_TRUE(EntityManager::has<int>(e));

    EXPECT_DEATH((void)EntityManager::get<char>(e), ".*FATAL.*Requested a non-existing component: Samir you're thrashing the cache");
    EXPECT_DEATH((void)EntityManager::clearComponent<char>(e), ".*FATAL.*Entity mask is missing components to delete");
}

TEST(EntityManagerTests, RemoveTest)
{
    const Entity e = EntityManager::create('a', 2);

    EntityManager::remove(e);

    EXPECT_DEATH(EntityManager::remove(e), ".*FATAL.*Attempting to delete an invalid entity");
    EXPECT_DEATH(EntityManager::remove(e + 1), ".*FATAL.*Attempting to delete a non-existant entity");

    EXPECT_FALSE(EntityManager::has<char>(e));
    EXPECT_FALSE(EntityManager::has<int>(e));
    EXPECT_FALSE(EntityManager::has<bool>(e));
}

}