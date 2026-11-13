#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

namespace hksr::api::detail {
inline std::string InstalledCatalogRevision(const std::filesystem::path& root) {
    std::map<std::string, std::string> markers;
    for (const auto* folder : {"StreamingAssets", "Persistent"}) {
        for (const auto* family : {"DesignData", "IFix"}) {
            const auto relative = std::string(folder) + "/" + family + "/Windows/" +
                                  (std::string(family) == "DesignData" ? "M_DesignV.bytes" : "M_IFixV.bytes");
            const auto file = root / std::filesystem::u8path(relative);
            if (!std::filesystem::exists(file)) {
                markers[relative] = "absent";
                continue;
            }
            std::ifstream in(file, std::ios::binary);
            if (!in)
                throw std::runtime_error("cannot read catalog revision: " + file.u8string());
            const std::string bytes((std::istreambuf_iterator<char>(in)), {});
            if (in.bad())
                throw std::runtime_error("catalog revision read failed: " + file.u8string());
            std::string value = "bytes:";
            for (const unsigned char byte : bytes) {
                constexpr char digits[] = "0123456789abcdef";
                value += digits[byte >> 4];
                value += digits[byte & 15];
            }
            markers[relative] = std::move(value);
        }
    }
    if (markers.at("StreamingAssets/DesignData/Windows/M_DesignV.bytes") == "absent" &&
        markers.at("Persistent/DesignData/Windows/M_DesignV.bytes") == "absent")
        throw std::runtime_error("installed catalog revision unavailable");
    std::string result;
    for (const auto& [path, value] : markers)
        result += path + "=" + value + "\n";
    return result;
}

inline std::string CatalogRevisionKey(const std::string& build, std::uint32_t design, const std::string& installed) {
    if (installed.empty())
        throw std::invalid_argument("installed catalog revision is empty");
    return "catalog:1:" + std::to_string(build.size()) + ":" + build + ":" + std::to_string(design) + ":" +
           std::to_string(installed.size()) + ":" + installed;
}
}
