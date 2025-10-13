#pragma once

#include <filesystem>
#include <misc/utils.hpp>

namespace NH3D {

class Scene {
    NH3D_NO_COPY_MOVE(Scene)
public:
    Scene(const std::filesystem::path& filePath);

};

}