#pragma once

#include <misc/math.hpp>
#include <misc/types.hpp>

namespace NH3D {

// Could probably live outside of the ECS
struct CameraComponent {
    [[nodiscard]] float getFovY() const;

    void setFovY(float fovY);

    [[nodiscard]] float getNear() const;

    void setNear(float near);

    [[nodiscard]] float getFar() const;

    void setFar(float far);

    [[nodiscard]]
    mat4 getProjectionMatrix(float aspectRatio) const;

private:
    float fovY = 1.0467f; // 60° in radians
    float near = 0.1f;
    float far = 10000.0f;

    mutable mat4 projectionMatrix;
    mutable float lastAspectRatio = 1.0f;
    mutable bool dirtyPerspective = true;
};

}