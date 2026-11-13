#include "AttachmentVisibilityNative.hpp"
#include "../../../adapters/presentation/AttachmentVisibilityBindings.hpp"
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <map>
#include <stdexcept>

namespace hksr::api::detail {
namespace {
struct NativeVisibility : AttachmentVisibilityBindings {
    ObjectMetadata& metadata;
    ManagedObjects& objects;
    ManagedArrays arrays;
    ManagedRoots roots;
    std::vector<ManagedRootId> held, temporary;
    std::map<std::string, void*> texts;
    void* component = nullptr;
    bool component_retained = false;
    std::function<bool()> current;
    NativeVisibility(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, ReferenceMethods r, ManagedArrays a,
                     std::function<bool()> c)
        : AttachmentVisibilityBindings(ResolveAttachmentVisibility(m)), metadata(d), objects(o), arrays(std::move(a)),
          roots(r), current(std::move(c)) {}
    void Check() {
        if (!current())
            throw std::runtime_error("attachment model ended or replaced");
    }
    void PrepareInputs() {
        Check();
        if (!component_retained) {
            Keep({{true, 0}, component}, true);
            Check();
            component_retained = true;
        }
    }
    void* Keep(ObjectResult value, bool permanent = false, bool nullable = false) {
        if (!value.call.returned || (!nullable && !value.object))
            throw std::runtime_error("attachment visibility object unavailable");
        if (!value.object)
            return nullptr;
        auto& ids = permanent ? held : temporary;
        ids.reserve(ids.size() + 1);
        ids.push_back(roots.Keep(value.object));
        if (!roots.Status(ids.back()).ready)
            throw std::runtime_error("attachment visibility reference retention failed");
        return value.object;
    }
    void* Text(const std::string& text) {
        auto it = texts.find(text);
        if (it != texts.end())
            return it->second;
        auto* value = Keep(objects.NewStringUtf8(text.c_str()), true);
        texts.emplace(text, value);
        return value;
    }
    template <class... A> void* Object(Method method, A... args) {
        Check();
        ObjectResult value;
        value.call = native::Call(method, &value.object, args...);
        return Keep(value, false, true);
    }
    bool Release(std::vector<ManagedRootId>& ids) {
        for (auto it = ids.begin(); it != ids.end();) {
            if (roots.Release(*it).complete) {
                roots.Forget(*it);
                it = ids.erase(it);
            } else
                ++it;
        }
        return ids.empty();
    }
    bool Read(const std::string& point) {
        PrepareInputs();
        struct Cleanup {
            NativeVisibility& s;
            ~Cleanup() {
                s.Release(s.temporary);
            }
        } cleanup{*this};
        auto* name = Text(point);
        auto* values = Object(list, component);
        // Absence means no explicit attachment hide. The official setter's
        // visible=true path withdraws that hide, retaining prototype visibility.
        if (!values)
            return true;
        const auto to_array = AttachmentVisibilitySnapshot(metadata, values);
        auto* copy = Object(to_array, values);
        if (!copy)
            throw std::runtime_error("attachment visibility snapshot absent");
        const auto length = arrays.Length(copy);
        if (!length.read.returned || length.value < 0)
            throw std::runtime_error("attachment visibility snapshot unread");
        for (int i = 0; i < length.value; ++i) {
            auto* item = Keep(arrays.Get(copy, i));
            auto* text = AttachmentVisibilityName(metadata, item);
            bool same = false;
            Check();
            if (!native::Call(equal, &same, text, name).returned)
                throw std::runtime_error("attachment name comparison failed");
            if (same)
                return AttachmentVisibilityValue(metadata, item);
        }
        return true;
    }
    void Write(const std::string& point, bool visible, int delay) {
        PrepareInputs();
        auto* text = Text(point);
        Check();
        bool renderer_changed = false;
        // False also means unchanged/not yet rendered. It is not call failure.
        if (!native::Call(setter, &renderer_changed, component, text, visible, delay).returned)
            throw std::runtime_error("attachment visibility submission failed");
    }
    bool Close() {
        return Release(temporary) && Release(held);
    }
};
} // namespace
std::unique_ptr<AttachmentVisibility> NativeAttachmentVisibility(MethodResolver& methods, ObjectMetadata& metadata,
                                                                 ManagedObjects& objects, ReferenceMethods references,
                                                                 ManagedArrays arrays, void* component,
                                                                 std::function<bool()> current) {
    auto native = std::make_shared<NativeVisibility>(methods, metadata, objects, references, std::move(arrays),
                                                     std::move(current));
    native->component = component;
    return std::make_unique<AttachmentVisibility>(AttachmentVisibilityServices{
        [native] { return native->current(); }, [native](const auto& key) { return native->Read(key); },
        [native](const auto& key, bool visible, int delay) { native->Write(key, visible, delay); },
        [native] { return native->Close(); }});
}
} // namespace hksr::api::detail
