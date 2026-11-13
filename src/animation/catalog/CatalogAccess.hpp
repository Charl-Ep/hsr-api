#pragma once
#include <hksr/animation/OfficialCatalog.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../../metadata/InstanceField.hpp"
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <set>
#include <map>
#include <optional>
#include <chrono>

namespace hksr::api {
namespace catalog_detail {
inline void Require(bool value, const char* reason) {
    if (!value)
        throw std::runtime_error(reason);
}
inline Method Getter(const DeclaredType& type, const char* name) {
    Method result{};
    unsigned matches = 0;
    for (const auto& method : type.methods)
        if (method.name == name && !method.is_static && method.parameters.empty()) {
            ++matches;
            result = method.entry;
        }
    Require(matches == 1 && result.code, "collection getter missing or ambiguous");
    return result;
}
inline Method Instance(const DeclaredType& type, const char* name, const char* result,
                       std::initializer_list<const char*> parameters = {}) {
    MetadataResolver resolver({type});
    return resolver.Resolve(type.name_space.c_str(), type.name.c_str(), name, result, parameters);
}
inline DeclaredField Field(const std::vector<DeclaredField>& fields, const char* name, const char* type) {
    DeclaredField result;
    unsigned matches = 0;
    for (const auto& field : fields)
        if (field.name == name && field.type == type && field.flags_observed && !(field.flags & 0x10)) {
            ++matches;
            result = field;
        }
    Require(matches == 1, "declared instance field missing or ambiguous");
    return result;
}
template <class T> T Read(void* object, const DeclaredField& field) {
    T value{};
    Require(object && native::CopyMemoryChecked(&value, static_cast<unsigned char*>(object) + field.offset, sizeof(T)),
            "declared field read failed");
    return value;
}
}
using namespace catalog_detail;
namespace detail {
struct CatalogAccess {
    MethodResolver& methods;
    ObjectMetadata& metadata;
    ManagedObjects& objects;
    ReferenceMethods references;
    ManagedRoots roots;
    ManagedArrays arrays;
    ComponentConfig text_reader;
    std::vector<ManagedRootId> pending;
    DeclaredField FieldFor(void* object, const char* name, const char* type) {
        return ResolveInstanceField(metadata, object, name, type);
    }
    struct Scope {
        CatalogAccess& self;
        std::vector<ManagedRootId> ids;
        explicit Scope(CatalogAccess& s) : self(s) {}
        void ReleaseOne() {
            auto id = ids.back();
            ids.pop_back();
            if (id) {
                if (self.roots.Release(id).complete)
                    self.roots.Forget(id);
                else
                    self.pending.push_back(id);
            }
        }
        ~Scope() {
            while (!ids.empty())
                ReleaseOne();
        }
        void* Keep(ObjectResult result) {
            Require(result.call.returned && result.object, "official object result missing");
            ids.push_back(0);
            auto id = self.roots.Keep(result.object);
            ids.back() = id;
            Require(self.roots.Status(id).ready, "temporary managed reference unavailable");
            return result.object;
        }
    };
    CatalogAccess(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, ReferenceMethods r, ManagedArrays a,
                  ComponentConfig t)
        : methods(m), metadata(d), objects(o), references(r), roots(r), arrays(a), text_reader(t) {}

    template <class... Args> ObjectResult Call(Method method, Args... args) {
        ObjectResult result;
        result.call = native::Call(method, &result.object, args...);
        return result;
    }
    std::string Text(void* value) {
        if (!value)
            return {}; // optional strings may be null; not a read failure
        auto result = text_reader.ReadText(value);
        Require(result.read.returned, "managed text read failed");
        return result.utf8;
    }
    std::string String(void* value, const std::vector<DeclaredField>& fields, const char* name) {
        return Text(Read<void*>(value, Field(fields, name, "System.String")));
    }

    TextArrayInput Strings(void* value, const std::vector<DeclaredField>& fields, const char* name) {
        TextArrayInput output;
        try {
            auto array = Read<void*>(value, Field(fields, name, "System.String[]"));
            if (array) {
                auto count = arrays.Length(array);
                Require(count.read.returned && count.value >= 0, "string array length unavailable");
                for (int i = 0; i < count.value; ++i) {
                    auto item = arrays.Get(array, i);
                    Require(item.call.returned, "string array element unavailable");
                    output.values.push_back(Text(item.object));
                }
            }
            output.available = true;
        } catch (const std::exception& error) {
            output.error = error.what();
            output.values.clear();
        }
        return output;
    }
    void* Sequence(void* object, const std::vector<DeclaredField>& fields, const char* name, Scope& scope,
                   const char* element = nullptr) {
        DeclaredField selected;
        unsigned matches = 0;
        for (const auto& field : fields)
            if (field.name == name && field.flags_observed && !(field.flags & 0x10)) {
                selected = field;
                ++matches;
            }
        Require(matches == 1, "sequence instance declaration missing or ambiguous");
        const auto& type = selected.type;
        const std::string prefix = "System.Collections.Generic.List<";
        const bool array = type.size() > 2 && type.substr(type.size() - 2) == "[]";
        const bool list = type.compare(0, prefix.size(), prefix) == 0 && type.back() == '>';
        Require(array || list, "sequence container is neither declared array nor List");
        if (element)
            Require(type == (std::string(element) + "[]") || type == prefix + element + ">",
                    "sequence element type differs");
        auto value = Read<void*>(object, selected);
        if (!value || array)
            return value;
        auto declaration = metadata.ReadObjectType(value);
        int count = 0;
        Require(native::Call(Instance(declaration, "get_Count", "System.Int32"), &count, value).returned && count >= 0,
                "sequence count unavailable");
        auto object_type = scope.Keep(objects.ManagedType("System", "Object"));
        auto copy = scope.Keep(arrays.Create(object_type, count));
        // ICollection.CopyTo to Object[] boxes value elements. No generic
        // get_Item<T> invocation with a guessed reference-return ABI.
        Require(native::CallVoid(Instance(declaration, "System.Collections.ICollection.CopyTo", "System.Void",
                                          {"System.Array", "System.Int32"}),
                                 value, copy, 0)
                    .returned,
                "sequence copy failed");
        return copy;
    }
    template <class Row, class Decode>
    CatalogBatch<Row> Rows(void* object, const std::vector<DeclaredField>& fields, const char* name, Decode decode,
                           bool read_fields = true) {
        CatalogBatch<Row> output;
        Scope scope(*this);
        try {
            auto array = Sequence(object, fields, name, scope);
            if (array) {
                auto count = arrays.Length(array);
                Require(count.read.returned && count.value >= 0, "sequence length unavailable");
                output.declared_count = count.value;
                for (int i = 0; i < count.value; ++i) {
                    ++output.visited_count;
                    Scope row_scope(*this);
                    try {
                        auto item = arrays.Get(array, i);
                        Require(item.call.returned, "sequence item unavailable");
                        if (!item.object)
                            continue;
                        auto row = row_scope.Keep(item);
                        output.rows.push_back(decode(
                            row, read_fields ? metadata.ReadObjectFields(row) : std::vector<DeclaredField>{}, i));
                    } catch (const std::exception& error) {
                        output.issues.push_back({i, name, error.what()});
                    }
                }
            }
            output.enumerated = true;
        } catch (const std::exception& error) {
            output.issues.push_back({-1, name, error.what()});
        }
        return output;
    }
    ObjectResult StaticValue(Scope& scope, const char* space, const char* owner, const char* field_name) {
        auto* type = scope.Keep(objects.ManagedType(space, owner));
        auto* name = scope.Keep(objects.NewStringUtf8(field_name));
        auto get_field = methods.Resolve("System", "RuntimeType", "GetField", "System.Reflection.FieldInfo",
                                         {"System.String", "System.Reflection.BindingFlags"});
        auto* field = scope.Keep(Call(get_field, type, name, 56)); // Static | Public | NonPublic
        const auto declared = metadata.ReadObjectType(field);
        return Call(Instance(declared, "GetValue", "System.Object", {"System.Object"}), field,
                    static_cast<void*>(nullptr));
    }
};
}
}
