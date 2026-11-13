#include <hksr/metadata/MetadataResolver.hpp>
#include <stdexcept>

namespace hksr::api {
struct MetadataResolver::Storage {
    std::vector<DeclaredType> types;
};
MetadataResolver::MetadataResolver(const std::vector<DeclaredType>& declarations)
    : storage_(new Storage{declarations}) {}
MetadataResolver::~MetadataResolver() {
    delete storage_;
}
Method MetadataResolver::Resolve(const char* space, const char* type, const char* name, const char* result,
                                 std::initializer_list<const char*> parameters, MethodKind kind) {
    const DeclaredType* selected = nullptr;
    for (const auto& item : storage_->types)
        if (item.name_space == space && item.name == type) {
            if (selected)
                throw std::runtime_error("declared type is ambiguous");
            selected = &item;
        }
    if (!selected)
        throw std::runtime_error("declared type is missing");
    const std::vector<std::string> expected(parameters.begin(), parameters.end());
    if (!selected->iteration_error.empty())
        throw std::runtime_error("method inventory is incomplete: " + selected->iteration_error);
    for (const auto& issue : selected->method_issues) {
        const auto& partial = issue.declaration;
        if (!partial.name.empty() && partial.name != name)
            continue;
        if (!partial.result.empty() && partial.result != result)
            continue;
        if (issue.parameter_count && *issue.parameter_count != expected.size())
            continue;
        if (partial.parameters.size() > expected.size())
            continue;
        bool differs = false;
        for (std::size_t i = 0; i < partial.parameters.size(); ++i)
            if (partial.parameters[i] != expected[i]) {
                differs = true;
                break;
            }
        if (differs)
            continue;
        // An unread declaration that could match is not evidence of a unique
        // callable. Unrelated named/signature failures do not block this call.
        throw std::runtime_error(std::string("method declaration is incomplete: ") + type + "." + name + ": " +
                                 issue.error);
    }
    const DeclaredMethod* found = nullptr;
    for (const auto& method : selected->methods) {
        if (method.name != name || method.result != result || method.parameters != expected ||
            method.is_static != (kind == MethodKind::Static))
            continue;
        if (found)
            throw std::runtime_error("declared method is ambiguous");
        found = &method;
    }
    if (!found || !found->entry.code)
        throw std::runtime_error("declared method is missing or unbound");
    return found->entry;
}
}
