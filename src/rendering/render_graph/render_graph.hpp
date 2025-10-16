#pragma once

#include <misc/utils.hpp>
#include <rendering/core/rhi.hpp>

namespace NH3D {

class Scene;

class Command {
};

class RenderGraph {
    NH3D_NO_COPY_MOVE(RenderGraph)
public:
    RenderGraph(const Scene& scene);

private:
    // std::vector<RenderingCommand*> _commands;
};

}