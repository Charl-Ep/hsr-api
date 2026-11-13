#include "../../src/presentation/attachments/AttachmentVisibilityNative.hpp"
#include <Windows.h>
#include <map>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <functional>
using namespace hksr::api;
namespace {
int info, component, next = 0, writes = 0;
bool getter_null = false, current = true, fields_fail = false;
bool text_fail = false, free_fault = false;
int free_calls = 0;
std::function<void()> on_keep, on_write;
const char* row_type = "RPG.GameCore.CharacterVisibleComponent.AttachmentVisibilityInfo";
struct Row {
    const char* name;
    bool visible;
};
Row weapon{"Weapon", false};
std::vector<Row> rows{weapon};
std::map<int, void*> roots;
std::vector<std::unique_ptr<std::string>> strings;
void Check(bool ok) {
    if (!ok)
        throw std::runtime_error("native attachment visibility regression");
}
template <class F> Method M(F f) {
    return {reinterpret_cast<void*>(f), &info};
}
int __fastcall Keep(void* value, int, int, void*) {
    const auto id = ++next;
    roots[id] = value;
    if (on_keep) {
        auto callback = std::move(on_keep);
        on_keep = {};
        callback();
    }
    return id;
}
void* __fastcall Target(int id, void*) {
    return roots.at(id);
}
void __fastcall Free(int id, void*) {
    ++free_calls;
    if (free_fault)
        RaiseException(0xE0424242, 0, 0, nullptr);
    roots.erase(id);
}
void* __fastcall List(void* self, void*) {
    Check(self == &component);
    return getter_null ? nullptr : &rows;
}
void* __fastcall Copy(void* list, void*) {
    Check(list == &rows);
    return &rows;
}
int __fastcall Length(void* array, void*) {
    Check(array == &rows);
    return static_cast<int>(rows.size());
}
void* __fastcall Item(void* array, int index, void*) {
    Check(array == &rows);
    return &rows.at(index);
}
bool __fastcall Equal(const char* left, std::string* right, void*) {
    return left && *right == left;
}
bool __fastcall Set(void* self, std::string* name, bool visible, int delay, void*) {
    Check(self == &component && delay >= 0);
    ++writes;
    if (on_write) {
        auto callback = std::move(on_write);
        on_write = {};
        callback();
    }
    for (auto& row : rows)
        if (*name == row.name) {
            row.visible = visible;
            return false;
        }
    // Keep text ownership in this fixture, as the managed list does in game.
    rows.push_back({name->c_str(), visible});
    return false;
}
struct Provider : MethodResolver, ObjectMetadata, ManagedObjects {
    Method Resolve(const char* ns, const char* type, const char* name, const char* result,
                   std::initializer_list<const char*> parameters, MethodKind kind) override {
        MetadataResolver resolver(
            {{"RPG.GameCore",
              "CharacterVisibleComponent",
              {{"SetAttachmentVisibility",
                "System.Boolean",
                {"System.String", "System.Boolean", "System.Int32"},
                false,
                M(Set)},
               {"get_AttachmentVisibilityList",
                std::string("System.Collections.Generic.List<") + row_type + ">",
                {},
                false,
                M(List)}}},
             {"System", "String", {{"Equals", "System.Boolean", {"System.String", "System.String"}, true, M(Equal)}}}});
        return resolver.Resolve(ns, type, name, result, parameters, kind);
    }
    DeclaredType ReadObjectType(void* object) override {
        Check(object == &rows);
        return {
            "System.Collections.Generic", "List`1", {{"ToArray", std::string(row_type) + "[]", {}, false, M(Copy)}}};
    }
    std::vector<DeclaredField> ReadObjectFields(void*) override {
        if (fields_fail)
            throw std::runtime_error("field snapshot failed");
        return {{"", "AttachmentVisibilityInfo", "AttachmentName", "System.String", offsetof(Row, name), 6, true},
                {"", "AttachmentVisibilityInfo", "Visible", "System.Boolean", offsetof(Row, visible), 6, true}};
    }
    ObjectResult NewStringUtf8(const char* text) override {
        if (text_fail)
            return {};
        strings.push_back(std::make_unique<std::string>(text));
        return {{true, 0}, strings.back().get()};
    }
    ObjectResult ManagedType(const char*, const char*) override {
        throw std::runtime_error("unused Type allocation");
    }
    ObjectResult AllocateObject(const char*, const char*) override {
        throw std::runtime_error("unused object allocation");
    }
};
} // namespace
int main() {
    try {
        Provider provider;
        ReferenceMethods refs{M(Keep), M(Target), M(Free), {}, {}};
        ArrayMethods array;
        array.length = M(Length);
        array.get = M(Item);
        const auto make_session = [&] {
            return detail::NativeAttachmentVisibility(provider, provider, provider, refs, ManagedArrays(array),
                                                      &component, [] { return current; });
        };
        {
            auto deferred = make_session();
            Check(roots.empty());
            text_fail = true;
            bool failed = false;
            try {
                deferred->Set(1, "Weapon", true);
            } catch (const std::exception&) {
                failed = true;
            }
            text_fail = false;
            Check(failed && roots.size() == 1 && writes == 0);
            Check(deferred->Close() && roots.empty());
        }
        {
            auto replaced = make_session();
            const auto texts = strings.size();
            on_keep = [] { current = false; };
            bool failed = false;
            try {
                replaced->Set(1, "Weapon", true);
            } catch (const std::exception&) {
                failed = true;
            }
            Check(failed && roots.size() == 1 && strings.size() == texts && writes == 0);
            Check(replaced->Close() && roots.empty());
            current = true;
        }
        {
            auto expired = make_session();
            const auto keeps = next;
            current = false;
            bool failed = false;
            try {
                expired->Set(1, "Weapon", true);
            } catch (const std::exception&) {
                failed = true;
            }
            Check(failed && next == keeps && writes == 0 && expired->Close());
            current = true;
        }
        {
            auto preparing = make_session();
            const auto frees = free_calls;
            bool observed = false;
            on_keep = [&] {
                observed = true;
                Check(!preparing->Close() && !preparing->Release(1) && free_calls == frees);
            };
            bool stopped = false;
            try {
                preparing->Set(1, "Weapon", true);
            } catch (const std::exception&) {
                stopped = true;
            }
            Check(observed && stopped && writes == 0 && !roots.empty());
            Check(preparing->Close() && roots.empty());
        }
        {
            auto writing = make_session();
            bool observed = false;
            on_write = [&] {
                observed = true;
                const auto frees = free_calls;
                bool nested_rejected = false;
                try {
                    writing->Set(2, "Weapon", false);
                } catch (const std::logic_error&) {
                    nested_rejected = true;
                }
                Check(nested_rejected);
                Check(!writing->Close() && free_calls == frees);
            };
            writing->Set(1, "Weapon", true);
            Check(observed && rows[0].visible);
            Check(writing->Close() && !rows[0].visible && roots.empty());
        }
        auto session = detail::NativeAttachmentVisibility(provider, provider, provider, refs, ManagedArrays(array),
                                                          &component, [] { return current; });
        session->Set(1, "Weapon", true);
        Check(rows[0].visible);
        Check(session->Release(1) && !rows[0].visible); // original hidden state
        session->Set(2, "Hat", false);
        Check(rows.size() == 2 && !rows[1].visible);
        Check(session->Release(2) && rows[1].visible); // no prior explicit hide
        fields_fail = true;
        const auto before = writes;
        bool failed = false;
        try {
            session->Set(3, "Weapon", true);
        } catch (const std::exception&) {
            failed = true;
        }
        Check(failed && writes == before);
        fields_fail = false;
        session->Set(4, "Weapon", true);
        current = false;
        const auto ended = writes;
        Check(session->Close() && writes == ended && roots.empty());
        Check(session->Close());
        current = true;
        {
            auto unknown = make_session();
            unknown->Set(1, "Weapon", true);
            current = false;
            free_fault = true;
            Check(!unknown->Close());
            const auto frees = free_calls;
            free_fault = false;
            Check(!unknown->Close() && free_calls == frees);
        }
        roots.clear(); // fixture handles with unknown release; no production retry
        std::cout << "native attachment visibility lazy inputs, reentrant close and cleanup passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
