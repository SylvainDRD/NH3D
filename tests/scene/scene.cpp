#include <gtest/gtest.h>
#include <mock_rhi.hpp>
#include <scene/ecs/entity.hpp>
#include <scene/scene.hpp>

namespace NH3D::Test {

TEST(SceneTests, CreateTest)
{
    MockRHI rhi;
    Scene scene { rhi };
    EXPECT_DEATH((void)scene.get<int>(0), ".*FATAL.*");

    const Entity e = scene.create(3, 'a');
    EXPECT_EQ(scene.get<int>(e), 3);
    EXPECT_EQ(scene.get<char>(e), 'a');

    EXPECT_DEATH((void)scene.get<int>(e + 1), ".*FATAL.*");

    scene.remove(e);

    EXPECT_EQ(scene.create(1), e);
}

TEST(SceneTests, AddTest)
{
    MockRHI rhi;
    Scene scene { rhi };
    const Entity e1 = scene.create('a');
    const Entity e2 = scene.create('b');

    scene.add(e1, 1337);

    EXPECT_EQ(scene.get<int>(e1), 1337);
    EXPECT_EQ(scene.get<char>(e1), 'a');
    EXPECT_EQ(scene.get<char>(e2), 'b');
}

TEST(SceneTests, HasTest)
{
    MockRHI rhi;
    Scene scene { rhi };
    const Entity e1 = scene.create('a', 2);
    const Entity e2 = scene.create('b', true);

    EXPECT_DEATH((void)scene.checkComponents<int>(3), ".*FATAL.*");

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
    MockRHI rhi;
    Scene scene { rhi };
    const Entity e = scene.create('a', 2);

    scene.clearComponents<char>(e);
    EXPECT_FALSE(scene.checkComponents<char>(e));
    EXPECT_TRUE(scene.checkComponents<int>(e));

    EXPECT_DEATH((void)scene.get<char>(e), ".*FATAL.*");
    EXPECT_DEATH((void)scene.clearComponents<char>(e), ".*FATAL.*");
}

TEST(SceneTests, RemoveTest)
{
    MockRHI rhi;
    Scene scene { rhi };
    const Entity e = scene.create('a', 2);

    scene.remove(e);

    EXPECT_DEATH(scene.remove(e), ".*FATAL.*");
    EXPECT_DEATH(scene.remove(e + 1), ".*FATAL.*");

    EXPECT_DEATH((void)scene.checkComponents<char>(e), ".*FATAL.*");
    EXPECT_DEATH((void)scene.checkComponents<int>(e), ".*FATAL.*");
}

TEST(SceneTests, ClearAndReAddComponentTest)
{
    MockRHI rhi;
    Scene scene { rhi };
    const Entity e = scene.create('a', 2);

    scene.clearComponents<char>(e);
    EXPECT_FALSE(scene.checkComponents<char>(e));

    scene.add(e, 'b');

    EXPECT_TRUE(scene.checkComponents<char>(e));
    EXPECT_EQ(scene.get<char>(e), 'b');
    EXPECT_EQ(scene.get<int>(e), 2);
}

TEST(SceneTests, AddOnInvalidEntityDies)
{
    MockRHI rhi;
    Scene scene { rhi };
    const Entity e = scene.create(1);

    scene.remove(e);

    EXPECT_DEATH(scene.add(e, 2), ".*FATAL.*");
}

TEST(SceneTests, IsLeaf)
{
    MockRHI rhi;
    Scene scene { rhi };
    const Entity parent = scene.create(0);
    const Entity child1 = scene.create(1);
    const Entity child2 = scene.create(2);

    scene.setParent(child1, parent);
    scene.setParent(child2, parent);

    EXPECT_FALSE(scene.isLeaf(parent));
    EXPECT_TRUE(scene.isLeaf(child1));
    EXPECT_TRUE(scene.isLeaf(child2));

    scene.remove(child1);
    EXPECT_FALSE(scene.isLeaf(parent));
    EXPECT_DEATH((void)scene.isLeaf(child1), ".*FATAL.*");
    EXPECT_TRUE(scene.isLeaf(child2));

    scene.remove(child2);
    EXPECT_TRUE(scene.isLeaf(parent));
    EXPECT_DEATH((void)scene.isLeaf(child1), ".*FATAL.*");
    EXPECT_DEATH((void)scene.isLeaf(child2), ".*FATAL.*");

    EXPECT_DEATH((void)scene.isLeaf(999), ".*FATAL.*");

    scene.remove(parent);
    EXPECT_DEATH((void)scene.isLeaf(parent), ".*FATAL.*");
}

TEST(SceneTests, GetSubtreeInvalidEntityDies)
{
    MockRHI rhi;
    Scene scene { rhi };
    EXPECT_DEATH((void)scene.getSubtree(0), ".*FATAL.*");

    const Entity e = scene.create(1);
    scene.remove(e);
    EXPECT_DEATH((void)scene.getSubtree(e), ".*FATAL.*");
}

TEST(SceneTests, GetSetMainCamera)
{
    MockRHI rhi;
    Scene scene { rhi };

    EXPECT_EQ(scene.getMainCamera(), InvalidEntity);

    const Entity e1 = scene.create(CameraComponent {});
    const Entity e2 = scene.create(CameraComponent {});

    EXPECT_EQ(scene.getMainCamera(), e1);

    scene.setMainCamera(e2);
    EXPECT_EQ(scene.getMainCamera(), e2);

    scene.setMainCamera(e1);
    EXPECT_EQ(scene.getMainCamera(), e1);
}

} // namespace NH3D::Test