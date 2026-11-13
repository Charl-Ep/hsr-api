#include "../../../adapters/presentation/EffectSchemaBindings.hpp"
#include "EffectViewPolicy.hpp"
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>

namespace hksr::api::detail {
namespace {
template <class T> T Read(void* object, std::size_t offset) {
    T value{};
    if (!object || !native::CopyMemoryChecked(&value, static_cast<unsigned char*>(object) + offset, sizeof(value)))
        throw std::runtime_error("effect view field unreadable");
    return value;
}
struct Classifier {
    ObjectMetadata& metadata;
    TypeRelationships& relationships;
    ReferenceMethods refs;
    std::size_t ui = 0, list = 0, position = 0;
    EffectQueries::Fields fields;
    bool ui_prepared = false, plugins_prepared = false;
    std::string ui_error, plugins_error;
    std::unique_ptr<ManagedReferenceList> plugins;
    std::function<ReferenceListMethods(void*)> resolve_list;
    std::vector<std::unique_ptr<ManagedRoots>> unresolved;
    Classifier(ObjectMetadata& m, TypeRelationships& t, ReferenceMethods r,
               const EffectQueries::Fields& read_fields, std::function<ReferenceListMethods(void*)> resolve)
        : metadata(m), relationships(t), refs(r), fields(read_fields), resolve_list(std::move(resolve)) {}
    void PrepareUi() {
        if (!ui_prepared) {
            ui_prepared = true;
            try {
                ui = ResolveEffectViewUiOffset(fields);
            } catch (const std::exception& error) {
                ui_error = error.what();
            }
        }
        if (!ui_error.empty())
            throw std::runtime_error(ui_error);
    }
    void PreparePlugins() {
        if (!plugins_prepared) {
            plugins_prepared = true;
            try {
                const auto offsets = ResolveEffectViewPluginOffsets(fields);
                list = offsets.first;
                position = offsets.second;
            } catch (const std::exception& error) {
                plugins_error = error.what();
            }
        }
        if (!plugins_error.empty())
            throw std::runtime_error(plugins_error);
    }
    bool Classify(void* behavior, void* effect) {
        PrepareUi();
        if (Read<bool>(effect, ui))
            return true;
        PreparePlugins();
        void* collection = Read<void*>(behavior, list);
        // A not-yet-initialized list must not be cached as actor-local.
        if (!collection)
            throw std::runtime_error("effect plugin list not initialized");
        auto roots = std::make_unique<ManagedRoots>(refs);
        std::vector<ManagedRootId> held;
        const auto keep = [&](void* p) {
            held.reserve(held.size() + 1);
            held.push_back(roots->Keep(p));
            if (!roots->Status(held.back()).ready)
                throw std::runtime_error("effect classification root unavailable");
        };
        const auto release = [&] {
            if (!roots)
                return true;
            bool complete = true;
            for (auto id : held)
                if (roots->Release(id).complete)
                    roots->Forget(id);
                else
                    complete = false;
            if (!complete)
                unresolved.push_back(std::move(roots));
            else
                roots.reset();
            return complete;
        };
        try {
            keep(collection);
            if (!plugins) {
                auto methods = resolve_list(collection);
                if (!methods.binding_errors.empty())
                    throw std::runtime_error(methods.binding_errors.front());
                plugins = std::make_unique<ManagedReferenceList>(std::move(methods));
            }
            const auto count = plugins->Count(collection);
            if (!count.read.returned || count.value < 0)
                throw std::runtime_error("effect plugin count unreadable");
            bool camera = false;
            for (int i = 0; i < count.value && !camera; ++i) {
                const auto item = plugins->Item(collection, i);
                if (!item.call.returned)
                    throw std::runtime_error("effect plugin unreadable");
                if (!item.object)
                    continue;
                keep(item.object);
                const auto type = metadata.ReadObjectTypeName(item.object);
                if (IsEffectFollowPlugin(relationships, type))
                    camera = EffectViewPolicy::CameraSpace(false, Read<int>(item.object, position));
            }
            if (!release())
                throw std::runtime_error("effect classification GC cleanup unresolved; receipts retained");
            return camera;
        } catch (...) {
            release();
            throw;
        }
    }
};
}
EffectViewPolicy::Classify MakeEffectViewClassifier(ObjectMetadata& m, TypeRelationships& t, ReferenceMethods r,
                                                    EffectQueries::Fields fields,
                                                    std::function<ReferenceListMethods(void*)> resolve) {
    const auto state = std::make_shared<Classifier>(m, t, r, fields, std::move(resolve));
    return [state](void* behavior, void* effect) { return state->Classify(behavior, effect); };
}
}
