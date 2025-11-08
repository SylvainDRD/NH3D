#include "mesh_component.hpp"

namespace NH3D {

MeshComponent::MeshComponent(IRHI& rhi, const std::vector<VertexData>& vertexData, const std::vector<uint32>& indices)
{
    _vertexBuffer = rhi.createBuffer({ .size = vertexData.size() * sizeof(VertexData),
        .usage = BufferUsageFlagBits::STORAGE_BUFFER_BIT | BufferUsageFlagBits::DST_TRANSFER_BIT,
        .memory = BufferMemoryUsage::GPU_ONLY,
        .initialData = reinterpret_cast<const byte*>(vertexData.data()) });
    _indexBuffer = rhi.createBuffer({ .size = indices.size() * sizeof(uint32),
        .usage = BufferUsageFlagBits::INDEX_BUFFER_BIT | BufferUsageFlagBits::DST_TRANSFER_BIT,
        .memory = BufferMemoryUsage::GPU_ONLY,
        .initialData = reinterpret_cast<const byte*>(indices.data()) });
}

[[nodiscard]] Handle<Buffer> MeshComponent::getVertexBuffer() const { return _vertexBuffer; }

[[nodiscard]] Handle<Buffer> MeshComponent::getIndexBuffer() const { return _indexBuffer; }

}