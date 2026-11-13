#include "CatalogSchema.hpp"
#include "CatalogRevisionKey.hpp"
#include <hksr/animation/CatalogRevision.hpp>
#include <Windows.h>

namespace hksr::api {
std::string ReadInstalledCatalogRevision() {
    std::wstring executable(32768, L'\0');
    const auto size = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (!size || size >= executable.size())
        throw std::runtime_error("cannot resolve catalog data directory");
    executable.resize(size);
    const std::filesystem::path game(executable);
    return detail::InstalledCatalogRevision(game.parent_path() / (game.stem().wstring() + L"_Data"));
}
}

namespace hksr::api::detail {
ActionDataVersion CatalogSchema::ReadActionDataVersion() {
    auto& s = *this;
    Scope scope(s);
    ActionDataVersion out;
    out.build = s.Text(scope.Keep(s.Call(s.Static("VersionData", "get_BuildId", "System.String"))));
    auto* version = scope.Keep(s.StaticValue(scope, "RPG.Client", "GlobalVars", "s_VersionData"));
    auto* update =
        Read<void*>(version, s.FieldFor(version, "UpdateVersionInfoIns", catalog_bindings::update_version_type));
    Require(update, "runtime resource version not initialized");
    const auto query = s.methods.Resolve("", catalog_bindings::update_version_type,
                                         catalog_bindings::selected_design_version, "System.UInt32", {});
    Require(native::Call(query, &out.design, update).returned, "selected design revision unavailable");
    Require(!out.build.empty() && out.design != 0, "action data identity incomplete");
    return out;
}
std::string CatalogSchema::ReadCatalogRevision(const std::string& installed) {
    const auto version = ReadActionDataVersion();
    return CatalogRevisionKey(version.build, version.design, installed);
}
}
