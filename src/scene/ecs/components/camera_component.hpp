#pragma once

namespace NH3D {

// Could probably live outside of the ECS
struct CameraComponent {
    float fovY = 1.57f; // 90° in radians
    float near = 0.1f;
    float far = 10000.0f;
};

}