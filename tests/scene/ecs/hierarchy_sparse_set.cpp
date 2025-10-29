#include <gtest/gtest.h>
#include <scene/ecs/entity.hpp>
#include <scene/ecs/hierarchy_sparse_set.hpp>

namespace NH3D::Test {

TEST(HierarchySparseSetTests, ParentAssignmentAndLeafChecks)
{
    HierarchySparseSet set;

    set.setParent(2, 1);

    EXPECT_EQ(set.size(), 2);
    EXPECT_EQ(set.get(2).parent(), 1u);
    EXPECT_FALSE(set.isLeaf(1));
    EXPECT_TRUE(set.isLeaf(2));

    EXPECT_EQ(set.getRaw(0).parent(), InvalidEntity);
    EXPECT_EQ(set.getRaw(1).parent(), 1);
}

TEST(HierarchySparseSetTests, RemoveLeafAndNonLeafBehavior)
{
    HierarchySparseSet set;

    set.setParent(11, 10);

    EXPECT_DEATH(set.remove(10), ".*FATAL.*");

    set.remove(11);
    EXPECT_EQ(set.size(), 1);
    EXPECT_EQ(set.entities().front(), 10);

    set.remove(10);
    EXPECT_EQ(set.size(), 0);
}

TEST(HierarchySparseSetTests, DeleteSubtreeRemovesAllDescendants)
{
    HierarchySparseSet set;

    set.setParent(1, 0);
    set.setParent(2, 1);
    set.setParent(3, 1);
    set.setParent(4, 0);

    EXPECT_EQ(set.size(), 5);

    set.deleteSubtree(1);

    EXPECT_EQ(set.size(), 2);
    EXPECT_EQ(set.entities()[0], 0);
    EXPECT_EQ(set.entities()[1], 4);

    EXPECT_EQ(set.getRaw(0).parent(), InvalidEntity);
    EXPECT_EQ(set.getRaw(1).parent(), 0);

    set.deleteSubtree(0);
    EXPECT_EQ(set.size(), 0);
    EXPECT_TRUE(set.entities().empty());
    EXPECT_DEATH((void)set.getRaw(0), ".*FATAL.*");
}

TEST(HierarchySparseSetTests, ReparentingUpdatesParentAndLeafStatus)
{
    HierarchySparseSet set;

    set.setParent(2, 1);
    set.setParent(3, 1);

    set.setParent(3, 2);

    EXPECT_EQ(set.get(3).parent(), 2);
    EXPECT_FALSE(set.isLeaf(2));
    EXPECT_TRUE(set.isLeaf(3));

    EXPECT_EQ(set.getRaw(0).parent(), InvalidEntity);
    EXPECT_EQ(set.getRaw(1).parent(), 1);
    EXPECT_EQ(set.getRaw(2).parent(), 2);
}

TEST(HierarchySparseSetTests, ReparentingToRootMakesLeaf)
{
    HierarchySparseSet set;

    set.setParent(2, 1);
    set.setParent(3, 1);

    set.setParent(2, InvalidEntity);

    EXPECT_EQ(set.size(), 2);
    EXPECT_EQ(set.get(1).parent(), InvalidEntity);
    EXPECT_DEATH((void)set.get(2), ".*FATAL.*");
    EXPECT_TRUE(set.isLeaf(3));
}

TEST(HierarchySparseSetTests, ReparentingSubtreeMaintainsStructure)
{
    HierarchySparseSet set;

    set.setParent(2, 1);
    set.setParent(3, 1);
    set.setParent(4, 2);

    EXPECT_EQ(set.getRaw(0).parent(), InvalidEntity);
    EXPECT_EQ(set.getRaw(1).parent(), 1);
    EXPECT_EQ(set.getRaw(2).parent(), 1);
    EXPECT_EQ(set.getRaw(3).parent(), 2);

    EXPECT_EQ(set.entities()[0], 0);
    EXPECT_EQ(set.entities()[1], 1);
    EXPECT_EQ(set.entities()[2], 2);
    EXPECT_EQ(set.entities()[3], 4);

    set.setParent(2, 3);

    EXPECT_EQ(set.get(2).parent(), 3);
    EXPECT_EQ(set.get(4).parent(), 2);
    EXPECT_FALSE(set.isLeaf(3));
    EXPECT_FALSE(set.isLeaf(2));
    EXPECT_TRUE(set.isLeaf(4));

    EXPECT_EQ(set.getRaw(0).parent(), InvalidEntity);
    EXPECT_EQ(set.getRaw(1).parent(), 3);
    EXPECT_EQ(set.getRaw(2).parent(), 1);
    EXPECT_EQ(set.getRaw(3).parent(), 2);

    EXPECT_EQ(set.entities()[0], 1);
    EXPECT_EQ(set.entities()[1], 3);    
    EXPECT_EQ(set.entities()[2], 2);
    EXPECT_EQ(set.entities()[3], 4);
}

TEST(HierarchySparseSetTests, GetSubtreeReturnsCorrectEntities)
{
    HierarchySparseSet set;

    set.setParent(2, 1);
    set.setParent(3, 1);
    set.setParent(4, 2);

    SubtreeView subtree = set.getSubtree(2);

    std::vector<Entity> entities;
    for (const Entity entity : subtree) {
        entities.push_back(entity);
    }

    EXPECT_EQ(entities.size(), 2);
    EXPECT_EQ(entities[0], 2);
    EXPECT_EQ(entities[1], 4);

    EXPECT_EQ(set.getRaw(0).parent(), InvalidEntity);
    EXPECT_EQ(set.getRaw(1).parent(), 1);
    EXPECT_EQ(set.getRaw(2).parent(), 1);
    EXPECT_EQ(set.getRaw(3).parent(), 2);   

    EXPECT_EQ(set.entities()[0], 0);
    EXPECT_EQ(set.entities()[1], 1);
    EXPECT_EQ(set.entities()[2], 2);
    EXPECT_EQ(set.entities()[3], 4);
}

TEST(HierarchySparseSetTests, DeleteSubtreePreservesOtherSubtrees)
{
    HierarchySparseSet set;

    set.setParent(2, 1);
    set.setParent(3, 1);
    set.setParent(4, 0);
    set.setParent(5, 4);

    set.deleteSubtree(1);

    EXPECT_EQ(set.size(), 3);
    EXPECT_EQ(set.entities()[0], 0);
    EXPECT_EQ(set.entities()[1], 4);
    EXPECT_EQ(set.entities()[2], 5);

    EXPECT_EQ(set.getRaw(0).parent(), InvalidEntity);
    EXPECT_EQ(set.getRaw(4).parent(), 0);
    EXPECT_EQ(set.getRaw(5).parent(), 0);

    EXPECT_EQ(set.entities()[0], 0);
    EXPECT_EQ(set.entities()[1], 4);
    EXPECT_EQ(set.entities()[2], 5);
}

} // namespace NH3D::Test