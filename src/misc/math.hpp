#pragma once

#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <misc/types.hpp>

namespace NH3D {

// Angle conversions
using glm::degrees;
using glm::radians;

// Scalar/vector math
using glm::abs;
using glm::clamp;
using glm::max;
using glm::min;
using glm::sign;

// Interpolation
using glm::mix;
using glm::smoothstep;
using glm::step;

// Lerp alias (to glm::mix)
template <class A, class B> constexpr auto lerp(const A& a, const A& b, const B t) -> decltype(glm::mix(a, b, t))
{
    return glm::mix(a, b, t);
}

// Saturate helper: clamp to [0, 1]
template <class T> constexpr T saturate(T x) { return glm::clamp(x, T { 0 }, T { 1 }); }

// Rounding/truncation
using glm::ceil;
using glm::floor;
using glm::fract;
using glm::round;
using glm::trunc;

// Modulo
using glm::mod;

// Vector ops
using glm::cross;
using glm::distance;
using glm::dot;
using glm::faceforward;
using glm::length;
using glm::normalize;
using glm::reflect;
using glm::refract;

// Fast squared norms/distances
using glm::distance2;
using glm::length2;

// Matrix transforms
using glm::rotate;
using glm::scale;
using glm::translate;

using glm::lookAt;
using glm::ortho;

inline glm::mat4 perspective(float fovY, float aspect, float near, float far)
{
    glm::mat4 proj = glm::perspectiveLH_ZO(fovY, aspect, near, far);
    proj[1][1] *= -1; // Flip Y for Vulkan
    return proj;
}

// Matrix ops
using glm::determinant;
using glm::inverse;
using glm::transpose;

// Quaternion helpers
using glm::angleAxis;
using glm::conjugate;
using glm::normalize;
using glm::slerp;
using glm::toMat4;

} // namespace NH3D