#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <functional>
#include <string>
#include <stdexcept>

namespace hksr::api::detail {
inline bool IsEventTaskType(TypeRelationships& types, const std::string& name) {
    const auto dot = name.rfind('.');
    return types.TypeDerivesFrom(name.substr(0, dot).c_str(), name.substr(dot + 1).c_str(), "RPG.GameCore",
                                 "TaskConfig");
}
inline bool IsEventConfigCandidate(TypeRelationships& types, const std::string& name,
                                   const std::function<bool()>& is_task) {
    if (name.find('<') != std::string::npos) {
        if (name.find("RPG.GameCore.TaskConfig") != std::string::npos)
            throw std::runtime_error("unsupported task configuration container: " + name);
        return false;
    }
    if (name.find_first_of("[]&*") != std::string::npos)
        return false;
    const auto dot = name.rfind('.');
    const auto derives = [&](const char* ns, const char* base) {
        return types.TypeDerivesFrom(name.substr(0, dot).c_str(), name.substr(dot + 1).c_str(), ns, base);
    };
    return is_task() || ((name.compare(0, 13, "RPG.GameCore.") == 0 || derives("RPG.GameCore", "JsonConfig")) &&
                         !derives("System", "ValueType"));
}
inline bool IsTaskEnabledField(const DeclaredField& field) {
    return field.name == "TaskEnabled" && field.type == "System.Boolean";
}
}
