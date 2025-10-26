#include <gtest/gtest.h>
#include <scene/ecs/entity.hpp>
#include <scene/scene.hpp>

namespace NH3D::Test {

TEST(SceneTests, CreateTest)
{
    Scene scene;
    EXPECT_DEATH((void)scene.get<int>(0), ".*FATAL.*Requested a component for an entity without storage");

    const Entity e = scene.create(3, 'a');
    EXPECT_EQ(scene.get<int>(e), 3);
    EXPECT_EQ(scene.get<char>(e), 'a');

    EXPECT_DEATH((void)scene.get<int>(e + 1), ".*FATAL.*Requested a non-existing component: Samir you're thrashing the cache");

    scene.remove(e);

    EXPECT_EQ(scene.create(1), e);
}

TEST(SceneTests, AddTest)
{
    Scene scene;
    const Entity e1 = scene.create('a');
    const Entity e2 = scene.create('b');

    scene.add(e1, 1337);

    EXPECT_EQ(scene.get<int>(e1), 1337);
    EXPECT_EQ(scene.get<char>(e1), 'a');
    EXPECT_EQ(scene.get<char>(e2), 'b');
}

TEST(SceneTests, HasTest)
{
    Scene scene;
    const Entity e1 = scene.create('a', 2);
    const Entity e2 = scene.create('b', true);

    EXPECT_DEATH((void)scene.checkComponents<int>(3), ".*FATAL.*Trying to check ComponentMask for a non-existing entity");

    EXPECT_TRUE(scene.checkComponents<int>(e1));
    EXPECT_TRUE(scene.checkComponents<char>(e1));
    EXPECT_FALSE(scene.checkComponents<bool>(e1));
    EXPECT_TRUE(scene.checkComponents<char>(e2));
    EXPECT_TRUE(scene.checkComponents<bool>(e2));
    EXPECT_FALSE(scene.checkComponents<int>(e2));

    bool has = scene.checkComponents<int, char>(e1);
    EXPECT_TRUE(has);
    has = scene.checkComponents<int, bool>(e1);
    EXPECT_FALSE(has);
    has = scene.checkComponents<bool, char>(e2);
    EXPECT_TRUE(has);
    has = scene.checkComponents<int, char>(e2);
    EXPECT_FALSE(has);
}

TEST(SceneTests, ClearComponentTest)
{
    Scene scene;
    const Entity e = scene.create('a', 2);

    scene.clearComponents<char>(e);
    EXPECT_FALSE(scene.checkComponents<char>(e));
    EXPECT_TRUE(scene.checkComponents<int>(e));

    EXPECT_DEATH((void)scene.get<char>(e), ".*FATAL.*Requested a non-existing component: Samir you're thrashing the cache");
    EXPECT_DEATH((void)scene.clearComponents<char>(e), ".*FATAL.*Entity mask is missing components to delete");
}

TEST(SceneTests, RemoveTest)
{
    Scene scene;
    const Entity e = scene.create('a', 2);

    scene.remove(e);

    EXPECT_DEATH(scene.remove(e), ".*FATAL.*Attempting to delete an invalid entity");
    EXPECT_DEATH(scene.remove(e + 1), ".*FATAL.*Attempting to delete a non-existant entity");

    EXPECT_FALSE(scene.checkComponents<char>(e));
    EXPECT_FALSE(scene.checkComponents<int>(e));
    EXPECT_FALSE(scene.checkComponents<bool>(e));
}

}