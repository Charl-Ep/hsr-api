#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace hksr::sdk_v1::detail {
struct ExecutableBytes { const unsigned char* data; std::size_t size; std::uint32_t rva; };
struct MeshWriterLocations {
    std::uint32_t fixed_wrapper=0, weights=0, bindposes=0;
    std::uint32_t mesh_data=0, bindpose_data=0, bindpose_count=0, bindpose_capacity=0;
};
// Code-only locator, shared by loaded-module binding and offline tests. RVAs
// are outputs. This is a retained-version relation, not a cross-version proof.
MeshWriterLocations LocateMeshWriters(const std::vector<ExecutableBytes>& sections);
}
