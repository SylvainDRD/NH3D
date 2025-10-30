#include <gtest/gtest.h>
#include <misc/math.hpp>
#include <scene/ecs/components/transform_component.hpp>
#include <scene/scene.hpp>

namespace NH3D::Test {

static bool approxEqual(const vec3& a, const vec3& b, float eps = 1e-5f)
{
    return length2(a - b) <= eps * eps;
}

static bool approxEqual(const quat& a, const quat& b, float eps = 1e-5f)
{
    // q and -q represent the same rotation; compare absolute dot to 1
    float d = abs(dot(a, b));
    return abs(1.0f - d) <= eps;
}

TEST(TransformComponentTests, TranslatePropagatesAndTargetsSubtree)
{
    Scene scene;

    const Entity root = scene.create(TransformComponent {});
    const Entity childA = scene.create(TransformComponent {});
    const Entity childB = scene.create(TransformComponent {});
    const Entity grandChild = scene.create(TransformComponent {});

    scene.setParent(childA, root);
    scene.setParent(childB, root);
    scene.setParent(grandChild, childA);

    // Initial state
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(root).position(), vec3 { 0 }));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childA).position(), vec3 { 0 }));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childB).position(), vec3 { 0 }));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(grandChild).position(), vec3 { 0 }));

    // Translate whole tree from root
    const vec3 t1(1.0f, 2.0f, 3.0f);
    scene.get<TransformComponent>(root).translate(scene, root, t1);

    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(root).position(), t1));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childA).position(), t1));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childB).position(), t1));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(grandChild).position(), t1));

    // Translate subtree starting at childA
    const vec3 t2(-2.0f, 0.5f, 4.0f);
    scene.get<TransformComponent>(childA).translate(scene, childA, t2);

    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(root).position(), t1));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childB).position(), t1));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childA).position(), t1 + t2));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(grandChild).position(), t1 + t2));
}

TEST(TransformComponentTests, RotatePropagatesAndComposes)
{
    Scene scene;

    const Entity root = scene.create(TransformComponent {});
    const Entity childA = scene.create(TransformComponent {});
    const Entity childB = scene.create(TransformComponent {});
    const Entity grandChild = scene.create(TransformComponent {});

    scene.setParent(childA, root);
    scene.setParent(childB, root);
    scene.setParent(grandChild, childA);

    const quat identity { 1.0f, 0.0f, 0.0f, 0.0f };
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(root).rotation(), identity));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childA).rotation(), identity));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childB).rotation(), identity));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(grandChild).rotation(), identity));

    // Apply rotation rot1 to whole tree
    const quat rot1 = glm::angleAxis(radians(30.0f), vec3(0, 1, 0));
    scene.get<TransformComponent>(root).rotate(scene, root, rot1);

    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(root).rotation(), rot1));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childA).rotation(), rot1));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childB).rotation(), rot1));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(grandChild).rotation(), rot1));

    // Compose rotation again from root: new = rot1 * old
    scene.get<TransformComponent>(root).rotate(scene, root, rot1);
    const quat rot2 = rot1 * rot1;

    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(root).rotation(), rot2));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childA).rotation(), rot2));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childB).rotation(), rot2));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(grandChild).rotation(), rot2));

    // Rotate only childB subtree by rotB: new(childB) = rotB * (rot1 * rot1)
    const quat rotB = angleAxis(radians(90.0f), vec3(1, 0, 0));
    scene.get<TransformComponent>(childB).rotate(scene, childB, rotB);

    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childB).rotation(), rotB * rot2));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(root).rotation(), rot2));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childA).rotation(), rot2));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(grandChild).rotation(), rot2));
}

TEST(TransformComponentTests, ScalePropagatesAndComposes)
{
    Scene scene;

    const Entity root = scene.create(TransformComponent {});
    const Entity childA = scene.create(TransformComponent {});
    const Entity childB = scene.create(TransformComponent {});
    const Entity grandChild = scene.create(TransformComponent {});

    scene.setParent(childA, root);
    scene.setParent(childB, root);
    scene.setParent(grandChild, childA);

    const vec3 one { 1.0f, 1.0f, 1.0f };
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(root).scale(), one));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childA).scale(), one));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childB).scale(), one));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(grandChild).scale(), one));

    // Scale whole tree
    const vec3 scale1(2.0f, 3.0f, 4.0f);
    scene.get<TransformComponent>(root).scale(scene, root, scale1);

    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(root).scale(), scale1));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childA).scale(), scale1));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childB).scale(), scale1));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(grandChild).scale(), scale1));

    // Scale subtree at childA
    const vec3 scale2(0.5f, 2.0f, 0.25f);
    scene.get<TransformComponent>(childA).scale(scene, childA, scale2);

    const vec3 scale3 = scale1 * scale2;
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childA).scale(), scale3));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(grandChild).scale(), scale3));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(root).scale(), scale1));
    EXPECT_TRUE(approxEqual(scene.get<TransformComponent>(childB).scale(), scale1));
}

} // namespace NH3D::Test
