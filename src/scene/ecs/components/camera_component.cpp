#include "camera_component.hpp"

namespace NH3D {

[[nodiscard]] float CameraComponent::getFovY() const { return fovY; }

void CameraComponent::setFovY(float fovY)
{
    this->fovY = fovY;
    dirtyPerspective = true;
}

[[nodiscard]] float CameraComponent::getNear() const { return near; }

void CameraComponent::setNear(float near)
{
    this->near = near;
    dirtyPerspective = true;
}

[[nodiscard]] float CameraComponent::getFar() const { return far; }

void CameraComponent::setFar(float far)
{
    this->far = far;
    dirtyPerspective = true;
}

[[nodiscard]]
mat4 CameraComponent::getProjectionMatrix(float aspectRatio) const
{
    if (dirtyPerspective || aspectRatio != lastAspectRatio) {
        projectionMatrix = perspective(fovY, aspectRatio, near, far);
        dirtyPerspective = false;
        lastAspectRatio = aspectRatio;
    }
    return projectionMatrix;
}

}