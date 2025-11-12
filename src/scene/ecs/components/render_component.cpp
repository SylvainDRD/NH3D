#include "render_component.hpp"

namespace NH3D {

RenderComponent::RenderComponent(IRHI& rhi, const std::vector<VertexData>& vertexData, const std::vector<uint32>& indices)
{
    _meshData.vertexBuffer = rhi.createBuffer({ .size = vertexData.size() * sizeof(VertexData),
        .usage = BufferUsageFlagBits::STORAGE_BUFFER_BIT | BufferUsageFlagBits::DST_TRANSFER_BIT,
        .memory = BufferMemoryUsage::GPU_ONLY,
        .initialData = reinterpret_cast<const byte*>(vertexData.data()) });

    _meshData.indexBuffer = rhi.createBuffer({ .size = indices.size() * sizeof(uint32),
        .usage = BufferUsageFlagBits::INDEX_BUFFER_BIT | BufferUsageFlagBits::DST_TRANSFER_BIT,
        .memory = BufferMemoryUsage::GPU_ONLY,
        .initialData = reinterpret_cast<const byte*>(indices.data()) });
}

[[nodiscard]] Handle<Buffer> RenderComponent::getVertexBuffer() const { return _meshData.vertexBuffer; }

[[nodiscard]] Handle<Buffer> RenderComponent::getIndexBuffer() const { return _meshData.indexBuffer; }

}