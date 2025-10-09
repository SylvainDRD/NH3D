#pragma once

#include <misc/utils.hpp>
#include <rendering/core/rhi_concept.hpp>

namespace NH3D {

class Command {
    
};

class RenderGraph {
    NH3D_NO_COPY_MOVE(RenderGraph)
public:
    RenderGraph() = default;

    void clear();

private:
    // std::vector<RenderingCommand*> _commands;
};

}