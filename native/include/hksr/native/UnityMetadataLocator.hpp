#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace hksr::sdk_v1::native {
struct MetadataCodeSection {
    const unsigned char* data;
    std::size_t size;
    std::uint32_t rva;
};
struct MetadataTableLocation {
    std::uint32_t instruction, table, domain_name;
};
using ReadImageBytes = std::function<bool(std::uint32_t, void*, std::size_t)>;
// Observed in both retained 4.5 and 4.6 Unity images. This only locates the
// root-domain call/table relation; it does NOT qualify the table's functions,
// MethodInfo layout, or any game-facing interface for execution.
std::vector<MetadataTableLocation> LocateUnityMetadataTables(
    const std::vector<MetadataCodeSection>& code, std::size_t image_size,
    const ReadImageBytes& read);
}
