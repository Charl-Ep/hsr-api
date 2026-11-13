#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <algorithm>
#include <stdexcept>

namespace hksr::api::detail {
// A single instance-field contract, not a complete schema snapshot. Preserve
// uncertainty only when an unread declaration could match this request.
inline DeclaredField ResolveInstanceField(ObjectMetadata& metadata, void* object, const char* name, const char* type) {
    if (!object || !name || !type)
        throw std::invalid_argument("instance field input absent");
    DeclaredField selected;
    unsigned matches = 0;
    auto consider = [&](const DeclaredField& field, const std::string& error) {
        if (error.empty() && (field.name != name || field.type != type))
            return;
        if (!field.name.empty() && field.name != name)
            return;
        if (!field.type.empty() && field.type != type)
            return;
        if (field.flags_observed && (field.flags & 0x10))
            return;
        if (!error.empty() || !field.flags_observed)
            throw std::runtime_error(std::string("instance field declaration incomplete: ") + name + ": " +
                                     (error.empty() ? "static/instance flags unavailable" : error));
        ++matches;
        selected = field;
    };
    if (auto* runtime = dynamic_cast<RuntimeMetadata*>(&metadata)) {
        std::vector<RuntimeType> visited;
        for (auto owner = runtime->ObjectType(object); owner; owner = runtime->ParentType(owner)) {
            if (std::find(visited.begin(), visited.end(), owner) != visited.end())
                throw std::runtime_error("instance field parent chain is cyclic");
            visited.push_back(owner);
            const auto observed = runtime->ObserveFields(owner);
            if (!observed.iteration_error.empty())
                throw std::runtime_error(std::string("instance field inventory incomplete: ") + name + ": " +
                                         observed.iteration_error);
            for (const auto& row : observed.fields)
                consider(row.declaration, row.error);
        }
    } else {
        for (const auto& field : metadata.ReadObjectFields(object))
            consider(field, {});
    }
    if (matches != 1)
        throw std::runtime_error(std::string("instance field missing or ambiguous: ") + name);
    return selected;
}
}
