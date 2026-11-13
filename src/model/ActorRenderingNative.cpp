#include "ActorRenderingNative.hpp"
#include <hksr/model/ModelAccess.hpp>
#include "../../adapters/model/RenderingCalls.hpp"
#include <hksr/entity/LifecycleHooks.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <hksr/unity/SceneObjects.hpp>
#include <hksr/unity/OwnedUnityObjects.hpp>
#include "../runtime/RuntimeResident.hpp"
#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>
namespace hksr::api::detail {
namespace {
template <class T> T Read(void* object, std::size_t offset) {
    T value{};
    if (!object || !native::CopyMemoryChecked(&value, static_cast<char*>(object) + offset, sizeof(value)))
        throw std::runtime_error("actor rendering field unreadable");
    return value;
}
RenderingWriteResult Applied() {
    return {RenderingWriteState::Applied, {}};
}
RenderingWriteResult Retired() {
    return {RenderingWriteState::Retired, "owned rendering epoch retired"};
}
}
struct ActorRenderingFactory::State : RenderingBindings {
    struct Bound;
    std::shared_ptr<RuntimeMetadata> metadata;
    ReferenceMethods references;
    EntityAccess entities;
    ModelAccess models;
    ManagedArrays arrays;
    EntityOwner owner;
    ManagedRoots roots;
    SceneObjects objects;
    std::vector<ManagedRootId> held;
    std::vector<std::shared_ptr<Bound>> clients;
    bool closed = false;
    State(RuntimeMetadata& m, ReferenceMethods refs, EntityAccess e, ModelAccess model, ManagedArrays a,
          EntityOwner own)
        : metadata(RetainRuntimeMetadata(m)), references(refs), entities(std::move(e)), models(std::move(model)),
          arrays(std::move(a)), owner(std::move(own)), roots(refs), objects(ResolveSceneObjectMethods(m)) {}
    void* Type(const char* space, const char* name) {
        const auto t = metadata->ManagedType(space, name);
        if (!t.call.returned || !t.object)
            throw std::runtime_error(std::string("actor rendering type missing: ") + name);
        const auto id = roots.Keep(t.object);
        held.push_back(id);
        if (!roots.Status(id).ready)
            throw std::runtime_error("actor rendering Type retain failed");
        return t.object;
    }
    bool Owned(EntityToken token, GameWorld* world) const {
        if (closed || !token.entity || !world || !token.generation || TrackedEntities().IsDisposed(token) ||
            TrackedEntities().IsRetiring(token) || owner.OwnedToken(token.entity).generation != token.generation)
            return false;
        const auto now = entities.Read(token.entity);
        if (!now.owner_world.read.returned)
            throw std::runtime_error("actor rendering world unreadable");
        return now.owner_world.value == world;
    }
    struct Bound {
        std::shared_ptr<State> factory;
        EntityToken token;
        GameWorld* world;
        void *model = nullptr, *art = nullptr, *root = nullptr;
        ManagedRoots roots;
        std::map<void*, ManagedRootId> kept;
        std::map<std::string, void*> texts;
        std::set<std::string> hidden;
        void* effect = nullptr;
        std::set<void*> renderers;
        bool closed = false;
        Bound(std::shared_ptr<State> f, EntityToken t, GameWorld* w)
            : factory(std::move(f)), token(t), world(w), roots(factory->references) {}
        void Keep(void* p) {
            if (!p || kept.count(p))
                return;
            const auto id = roots.Keep(p);
            kept.emplace(p, id);
            if (!roots.Status(id).ready)
                throw std::runtime_error("actor rendering root failed");
        }
        bool Drop(void* p) {
            const auto it = kept.find(p);
            if (it == kept.end())
                return true;
            if (!roots.Release(it->second).complete)
                return false;
            roots.Forget(it->second);
            kept.erase(it);
            return true;
        }
        void* Text(const std::string& key) {
            if (const auto it = texts.find(key); it != texts.end())
                return it->second;
            const auto value = factory->metadata->NewStringUtf8(key.c_str());
            if (!value.call.returned || !value.object)
                throw std::runtime_error("actor rendering text allocation failed");
            Keep(value.object);
            texts[key] = value.object;
            return value.object;
        }
        RenderingEpoch Epoch() const {
            return {reinterpret_cast<std::uintptr_t>(model), reinterpret_cast<std::uintptr_t>(art),
                    reinterpret_cast<std::uintptr_t>(root)};
        }
        bool Current() {
            auto& f = *factory;
            if (closed || !f.Owned(token, world))
                return false;
            if (!f.model_error.empty())
                throw std::runtime_error(f.model_error);
            const auto actual = f.entities.Component(token.entity, f.model_type);
            if (!actual.call.returned)
                throw std::runtime_error("actor rendering model lookup failed");
            if (!actual.object || (model && actual.object != model))
                return false;
            const auto snapshot = f.models.Read(static_cast<CharacterModelComponent*>(actual.object));
            if (!snapshot.art.read.returned || !snapshot.root.read.returned || !snapshot.changing.read.returned)
                throw std::runtime_error("actor rendering model observation failed");
            if (snapshot.changing.value || !snapshot.art.value || !snapshot.root.value)
                return false;
            if (!model) {
                Keep(actual.object);
                Keep(snapshot.art.value);
                Keep(snapshot.root.value);
                model = actual.object;
                art = snapshot.art.value;
                root = snapshot.root.value;
            }
            return snapshot.art.value == art && snapshot.root.value == root && f.Owned(token, world);
        }
        bool Alive(void* value) {
            const auto read = UnityObjectAlive(factory->alive, value);
            if (!read.read.returned)
                throw std::runtime_error("actor rendering Unity lifetime unreadable");
            return read.value;
        }
        void* Component(void* value, void* type) {
            void* result = nullptr;
            if (!native::Call(factory->component, &result, value, type).returned)
                throw std::runtime_error("actor rendering Unity component lookup failed");
            Keep(result);
            return result;
        }
        std::vector<void*> Array(void* value) {
            std::vector<void*> out;
            if (!value)
                return out;
            Keep(value);
            const auto count = factory->arrays.Length(value);
            if (!count.read.returned || count.value < 0)
                throw std::runtime_error("actor renderer array count failed");
            try {
                for (int i = 0; i < count.value; ++i) {
                    const auto row = factory->arrays.Get(value, i);
                    if (!row.call.returned)
                        throw std::runtime_error("actor renderer array item failed");
                    if (row.object) {
                        Keep(row.object);
                        renderers.insert(row.object);
                        out.push_back(row.object);
                    }
                }
            } catch (...) {
                Drop(value);
                throw;
            }
            if (!Drop(value))
                throw std::runtime_error("temporary renderer array root did not drain");
            return out;
        }
        std::optional<std::vector<RendererMaskValue>> Renderers(const std::string& node) {
            if (!Current())
                return {};
            auto& f = *factory;
            if (!f.renderer_error.empty())
                throw std::runtime_error(f.renderer_error);
            if (!f.mask_read_error.empty())
                throw std::runtime_error(f.mask_read_error);
            void* target = root;
            if (!node.empty()) {
                if (!f.attachment_error.empty())
                    throw std::runtime_error(f.attachment_error);
                void* on_root = nullptr;
                if (!native::Call(f.root_component, &on_root, root, f.mapping_type).returned)
                    throw std::runtime_error("actor attachment mapping lookup failed");
                if (!on_root)
                    return {};
                Keep(on_root);
                void* point = nullptr;
                if (!native::Call(f.find_attach, &point, on_root, Text(node), false, static_cast<void*>(nullptr))
                         .returned)
                    throw std::runtime_error("actor renderer attachment lookup failed");
                if (!point)
                    return {};
                Keep(point);
                if (!native::Call(f.attachment_game_object, &target, point).returned)
                    throw std::runtime_error("actor renderer attachment root failed");
                if (!target)
                    return {};
                Keep(target);
            }
            const auto components = f.objects.Components(target, f.renderer_type, false);
            if (!components.call.returned)
                throw std::runtime_error("actor renderer hierarchy lookup failed");
            std::vector<RendererMaskValue> out;
            for (auto* renderer : Array(components.object)) {
                if (!Alive(renderer))
                    continue;
                std::uint32_t mask = 0;
                if (!native::Call(f.mask_read, &mask, renderer).returned)
                    throw std::runtime_error("renderer mask observation failed");
                out.push_back({Epoch(), reinterpret_cast<std::uintptr_t>(renderer), mask});
            }
            if (!Current())
                return {};
            return out;
        }
        RenderingWriteResult Mask(const RendererMaskValue& value, std::uint32_t mask) {
            if (!factory->mask_write_error.empty())
                return {RenderingWriteState::Failed, factory->mask_write_error};
            if (!(value.epoch == Epoch()) || !Current())
                return Retired();
            auto* renderer = reinterpret_cast<void*>(value.renderer);
            if (!renderers.count(renderer))
                return {RenderingWriteState::Failed, "renderer was not observed through this owned model"};
            if (!Alive(renderer))
                return Retired();
            if (!SetRenderingMask(factory->mask_write, renderer, mask).returned)
                return {RenderingWriteState::Failed, "renderer mask native write failed"};
            return Applied();
        }
        std::optional<std::uint32_t> ReadMask(const RendererMaskValue& value) {
            if (!factory->mask_read_error.empty())
                throw std::runtime_error(factory->mask_read_error);
            if (!(value.epoch == Epoch()) || !Current())
                return {};
            auto* renderer = reinterpret_cast<void*>(value.renderer);
            if (!renderers.count(renderer) || !Alive(renderer))
                return {};
            std::uint32_t mask = 0;
            if (!native::Call(factory->mask_read, &mask, renderer).returned)
                throw std::runtime_error("renderer mask read failed");
            return mask;
        }
        std::vector<std::uintptr_t> List(void* value) {
            std::vector<std::uintptr_t> out;
            if (!value)
                return out;
            Keep(value);
            const auto count = factory->list->Count(value);
            if (!count.read.returned || count.value < 0)
                throw std::runtime_error("outline list count failed");
            for (int i = 0; i < count.value; ++i) {
                const auto row = factory->list->Item(value, i);
                if (!row.call.returned)
                    throw std::runtime_error("outline list item failed");
                if (row.object) {
                    Keep(row.object);
                    renderers.insert(row.object);
                    out.push_back(reinterpret_cast<std::uintptr_t>(row.object));
                }
            }
            return out;
        }
        std::optional<OutlineTargets> Outline() {
            if (!Current())
                return {};
            auto& f = *factory;
            if (!f.renderer_error.empty())
                throw std::runtime_error(f.renderer_error);
            if (!f.outline_error.empty())
                throw std::runtime_error(f.outline_error);
            if (!effect) {
                const auto all = f.objects.Components(root, f.renderer_type, false);
                if (!all.call.returned)
                    throw std::runtime_error("outline renderer hierarchy lookup failed");
                for (auto* renderer : Array(all.object)) {
                    void* cursor = nullptr;
                    if (!native::Call(f.component_transform, &cursor, renderer).returned)
                        throw std::runtime_error("outline transform unreadable");
                    while (cursor) {
                        Keep(cursor);
                        effect = Component(cursor, f.effect_type);
                        if (effect)
                            break;
                        void* go = nullptr;
                        if (!native::Call(f.outline_game_object, &go, cursor).returned)
                            throw std::runtime_error("outline ancestor unreadable");
                        if (go == root)
                            break;
                        void* next = nullptr;
                        if (!native::Call(f.parent, &next, cursor).returned)
                            throw std::runtime_error("outline parent unreadable");
                        cursor = next;
                    }
                    if (effect)
                        break;
                }
            }
            if (!effect || !Alive(effect))
                return {};
            return OutlineTargets{Epoch(), reinterpret_cast<std::uintptr_t>(effect),
                                  List(Read<void*>(effect, f.outline_add)),
                                  List(Read<void*>(effect, f.outline_remove))};
        }
        std::optional<std::uintptr_t> Named(const std::string& name) {
            if (!Current())
                return {};
            auto& f = *factory;
            if (!f.named_error.empty())
                throw std::runtime_error(f.named_error);
            void* transition = nullptr;
            if (!native::Call(f.shader_property, &transition, token.entity).returned)
                throw std::runtime_error("outline renderer cache component unreadable");
            if (!transition)
                return {};
            Keep(transition);
            auto* cache = Read<void*>(transition, f.renderer_cache);
            if (!cache)
                return {};
            Keep(cache);
            void* renderer = nullptr;
            if (!native::Call(f.find_renderer, &renderer, cache, Text(name)).returned)
                throw std::runtime_error("outline named renderer lookup failed");
            if (!renderer)
                return {};
            Keep(renderer);
            renderers.insert(renderer);
            return reinterpret_cast<std::uintptr_t>(renderer);
        }
        void* MakeList(const std::vector<std::uintptr_t>& values) {
            auto& f = *factory;
            const auto result = f.metadata->AllocateObject(f.list_type);
            if (!result.call.returned || !result.object)
                throw std::runtime_error("outline list allocation failed");
            Keep(result.object);
            try {
                if (!native::CallVoid(f.list_ctor, result.object).returned)
                    throw std::runtime_error("outline list constructor failed");
                for (const auto value : values) {
                    auto* object = reinterpret_cast<void*>(value);
                    if (!renderers.count(object) || !Alive(object))
                        throw std::runtime_error("outline target renderer unavailable");
                    if (!native::CallVoid(f.list_add, result.object, object).returned)
                        throw std::runtime_error("outline list append failed");
                }
            } catch (...) {
                Drop(result.object);
                throw;
            }
            return result.object;
        }
        RenderingWriteResult OutlineWrite(const OutlineTargets& value, const std::vector<std::uintptr_t>& add,
                                          const std::vector<std::uintptr_t>& remove) {
            if (!factory->outline_write_error.empty())
                return {RenderingWriteState::Failed, factory->outline_write_error};
            if (!(value.epoch == Epoch()) || !Current())
                return Retired();
            if (value.effect != reinterpret_cast<std::uintptr_t>(effect) || !effect || !Alive(effect))
                return Retired();
            void* added = nullptr;
            void* removed = nullptr;
            CallResult called;
            try {
                added = MakeList(add);
                removed = MakeList(remove);
                called = SetRenderingOutline(factory->outline_set, effect, added, removed);
            } catch (...) {
                Drop(added);
                Drop(removed);
                throw;
            }
            // Both ordinary helpers enumerate input elements into their own
            // component lists; they do not retain these IReadOnlyList objects.
            const bool first = Drop(added), second = Drop(removed);
            if (!called.returned)
                return {RenderingWriteState::Failed, "outline native write failed; may be partial"};
            if (!first || !second)
                return {RenderingWriteState::Failed, "temporary outline list roots did not drain"};
            return Applied();
        }
        RenderingWriteResult Visible(const std::string& key, bool value) {
            auto& f = *factory;
            if (closed || !f.Owned(token, world))
                return Retired();
            if (!f.visibility_error.empty())
                return {RenderingWriteState::Failed, f.visibility_error};
            const auto visible = f.entities.Component(token.entity, f.visible_type);
            if (!visible.call.returned)
                return {RenderingWriteState::Failed, "visible component lookup failed"};
            if (!visible.object)
                return Retired();
            Keep(visible.object);
            auto* text = Text(key);
            if (!value)
                hidden.insert(key);
            if (!SetRenderingVisibility(f.visibility_set, visible.object, text, value).returned)
                return {RenderingWriteState::Failed, "visible key write failed; may be partial"};
            if (value) {
                hidden.erase(key);
                texts.erase(key);
                if (!Drop(text))
                    return {RenderingWriteState::Failed, "visible key root did not drain"};
            }
            return Applied();
        }
        bool Close() {
            if (!closed) {
                auto copy = hidden;
                for (const auto& key : copy)
                    if (Visible(key, true).state == RenderingWriteState::Failed)
                        return false;
                hidden.clear();
                closed = true;
            }
            for (auto it = kept.begin(); it != kept.end();)
                if (roots.Release(it->second).complete) {
                    roots.Forget(it->second);
                    it = kept.erase(it);
                } else
                    ++it;
            return kept.empty();
        }
    };
};
ActorRenderingFactory::ActorRenderingFactory(RuntimeMetadata& m, ReferenceMethods refs, EntityAccess entities,
                                             ModelAccess models, ManagedArrays arrays, EntityOwner owner)
    : state_(std::make_shared<State>(m, refs, std::move(entities), std::move(models), std::move(arrays),
                                     std::move(owner))) {
    auto& s = *state_;
    try {

        static_cast<RenderingBindings&>(s) =
            ResolveRenderingBindings(m, [&](const char* ns, const char* name) { return s.Type(ns, name); });
    } catch (...) {
        Close();
        throw;
    }
}
ActorRenderingFactory::~ActorRenderingFactory() = default;
std::unique_ptr<ActorRendering> ActorRenderingFactory::Create(EntityToken token, GameWorld* world) {
    auto s = state_;
    if (!s->Owned(token, world))
        throw std::runtime_error("actor rendering requires current owned actor");
    s->clients.erase(std::remove_if(s->clients.begin(), s->clients.end(),
                                    [](const auto& old) { return old->closed && old->kept.empty(); }),
                     s->clients.end());
    auto b = std::make_shared<State::Bound>(s, token, world);
    s->clients.push_back(b);
    b->Keep(token.entity);
    b->Keep(world);
    return std::make_unique<ActorRendering>(ActorRenderingServices{
        [b](const auto& node) { return b->Renderers(node); },
        [b](const auto& target, auto mask) { return b->Mask(target, mask); }, [b] { return b->Outline(); },
        [b](const auto& name) { return b->Named(name); },
        [b](const auto& target, const auto& add, const auto& remove) { return b->OutlineWrite(target, add, remove); },
        [b](const auto& key, bool visible) { return b->Visible(key, visible); }, [b] { return b->Close(); },
        [b] { return b->Current(); }, [b](const auto& value) { return b->ReadMask(value); }});
}
bool ActorRenderingFactory::Close() {
    auto& s = *state_;
    bool complete = true;
    for (auto it = s.clients.begin(); it != s.clients.end();)
        if ((*it)->Close())
            it = s.clients.erase(it);
        else {
            complete = false;
            ++it;
        }
    if (!s.clients.empty())
        return false;
    s.closed = true;
    for (auto it = s.held.begin(); it != s.held.end();)
        if (s.roots.Release(*it).complete) {
            s.roots.Forget(*it);
            it = s.held.erase(it);
        } else
            ++it;
    return complete && s.held.empty();
}
}
