#pragma once

#include <cstdint>
#include <filesystem>
#include <misc/types.hpp>
#include <misc/utils.hpp>
#include <rendering/core/rhi_interface.hpp>

namespace NH3D {

struct VertexData {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
};

template <RHI RHI>
struct MeshComponent {
    NH3D_NO_COPY(MeshComponent)
public:
    MeshComponent(RHI& rhi, const std::vector<VertexData>& vertexData, const std::vector<uint32_t>& indices);

    MeshComponent(RHI& rhi, const std::filesystem::path& glbPath);

private:
    RID _meshData;
    RID _indices;
};


template<RHI RHI>
MeshComponent<RHI>::MeshComponent(RHI& rhi, const std::vector<VertexData>& vertexData, const std::vector<uint32_t>& indices) {
    // TODO
}

template<RHI RHI>
MeshComponent<RHI>::MeshComponent(RHI& rhi, const std::filesystem::path& glbPath) {
    NH3D_ABORT("NOT IMPLEMENTED :(");
}

}
