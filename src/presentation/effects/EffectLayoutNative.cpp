#include "EffectLayoutNative.hpp"
#include "../../../adapters/presentation/EffectLayoutCalls.hpp"
#include "../../../adapters/presentation/EffectLayoutBindings.hpp"
#include "../../../adapters/presentation/EffectSchemaBindings.hpp"
#include <hksr/presentation/effects/EffectRelations.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/MetadataReader.hpp>
#include "../../runtime/RuntimeResident.hpp"
#include <stdexcept>
namespace hksr::api::detail {
namespace {
template <class T> T Read(void* object, std::size_t offset) {
    T value{};
    if (!object || !native::CopyMemoryChecked(&value, static_cast<char*>(object) + offset, sizeof(value)))
        throw std::runtime_error("layout field unreadable");
    return value;
}
template <class T> void Write(void* object, std::size_t offset, T value) {
    if (!object || !native::CopyMemoryChecked(static_cast<char*>(object) + offset, &value, sizeof(value)))
        throw std::runtime_error("layout scalar write failed; may be partial");
}
struct NativeLayouts : EffectLayoutNativeBindings {
    std::shared_ptr<RuntimeMetadata> metadata;
    EffectAccess access;
    EffectRelations relations;
    ManagedRoots roots;
    std::vector<ManagedRootId> types, temporary;
    bool closed = false, busy = false;
    NativeLayouts(EffectAccess a, EffectRelations r, ReferenceMethods refs)
        : access(std::move(a)), relations(std::move(r)), roots(refs) {}
    NativeLayouts(RuntimeMetadata& m, EffectAccess a, ReferenceMethods refs)
        : NativeLayouts(std::move(a), EffectRelations(ReadEffectRelationsFields(m)), refs) {
        metadata = RetainRuntimeMetadata(m);
    }
    void Keep(void* value, std::vector<ManagedRootId>& list) {
        if (!value)
            throw std::runtime_error("layout reference missing");
        const auto id = roots.Keep(value);
        list.push_back(id);
        if (!roots.Status(id).ready)
            throw std::runtime_error("layout reference retain failed");
    }
    void* Keep(ObjectResult value) {
        if (!value.call.returned || !value.object)
            throw std::runtime_error("layout metadata object unavailable");
        Keep(value.object, types);
        return value.object;
    }
    bool Release(std::vector<ManagedRootId>& list) {
        for (auto it = list.begin(); it != list.end();)
            if (roots.Release(*it).complete) {
                roots.Forget(*it);
                it = list.erase(it);
            } else
                ++it;
        return list.empty();
    }
    struct Layout {
        EffectReference ref;
        void* effect = nullptr;
        void* plugin = nullptr;
        void* driver = nullptr;
    };
    bool Current(const Layout& row, EffectControlResult& out) {
        const auto seen = access.Observe(row.ref);
        if (!seen.id_before.read.returned || !seen.id_after.read.returned || !seen.released.read.returned ||
            !seen.instance.call.returned)
            throw std::runtime_error("layout effect receipt unreadable");
        if (!seen.same_generation || seen.released.value) {
            out.state = EffectControlState::Ended;
            out.detail = "layout receipt retired or pooled";
            return false;
        }
        if (!seen.instance.object) {
            out.state = EffectControlState::NotLoaded;
            out.detail = "layout effect not loaded";
            return false;
        }
        if ((row.effect && seen.instance.object != row.effect) ||
            Read<void*>(seen.instance.object, trigger) != row.ref.parameters)
            throw std::runtime_error("layout effect request identity changed");
        return true;
    }
    void Alive(void* value) {
        bool live = false;
        if (!native::Call(alive, &live, value).returned)
            throw std::runtime_error("layout Unity liveness query failed");
        if (!live)
            throw std::runtime_error("layout plugin destroyed");
    }
    bool Bind(EffectReference ref, Layout& row, EffectControlResult& out) {
        row.ref = ref;
        Keep(ref.parameters, temporary);
        if (!Current(row, out))
            return false;
        const auto seen = access.Observe(ref);
        if (!seen.instance.call.returned || !seen.instance.object)
            throw std::runtime_error("layout instance changed during capture");
        row.effect = seen.instance.object;
        Keep(row.effect, temporary);
        Alive(row.effect);
        if (!native::Call(component, &row.plugin, row.effect, plugin_type).returned)
            throw std::runtime_error("layout component lookup failed");
        if (!row.plugin) {
            out.state = EffectControlState::Unsupported;
            out.detail = "effect has no Mahjong layout plugin";
            return false;
        }
        Keep(row.plugin, temporary);
        Alive(row.plugin);
        if (Read<void*>(row.plugin, plugin_owner) != row.effect)
            throw std::runtime_error("layout plugin owner differs");
        row.driver = Read<void*>(row.plugin, behavior);
        if (!row.driver) {
            out.state = EffectControlState::Unsupported;
            out.detail = "layout behavior unavailable";
            return false;
        }
        Keep(row.driver, temporary);
        if (Read<void*>(row.driver, behavior_owner) != row.plugin)
            throw std::runtime_error("layout behavior owner differs");
        return Current(row, out);
    }
    bool Pair(const Layout& from, const Layout& to, EffectControlResult& out) {
        if (!Current(from, out) || !Current(to, out))
            return false;
        const auto first = relations.Read(from.ref), second = relations.Read(to.ref);
        if (!first.target.call.returned || !second.target.call.returned || !first.same_generation ||
            !second.same_generation)
            throw std::runtime_error("layout transfer receiver unreadable");
        if (!first.target.object || first.target.object != second.target.object) {
            out.state = EffectControlState::Unsupported;
            out.detail = "layout transfer requires the same explicit actor receiver";
            return false;
        }
        return true;
    }
    EffectControlResult Execute(EffectReference ref, const EffectLayoutCommand& command) {
        EffectControlResult out;
        if (closed || busy) {
            out.detail = "layout service closed or reentered";
            return out;
        }
        busy = true;
        struct Guard {
            bool& value;
            ~Guard() {
                value = false;
            }
        } guard{busy};
        try {
            Layout from, to;
            if (Bind(ref, from, out)) {
                if (command.action == EffectLayoutAction::Transfer) {
                    if (Bind(command.target, to, out) && Pair(from, to, out)) {
                        if (!Read<bool>(from.plugin, enabled)) {
                            out.state = EffectControlState::NoMatch;
                            out.detail = "source layout is disabled";
                        } else {
                            Write(from.plugin, enabled, false);
                            out.call = LayoutCopy(copy, to.driver, from.plugin);
                            if (!out.call.returned)
                                throw std::runtime_error("layout transfer copy failed; may be partial");
                            if (Pair(from, to, out)) {
                                out.call = LayoutClear(clear, from.driver);
                                if (!out.call.returned)
                                    throw std::runtime_error("layout transfer clear failed; may be partial");
                                if (Pair(from, to, out)) {
                                    out.call = LayoutReference(inherit_set, inherit_field, from.plugin, to.plugin);
                                    if (!out.call.returned)
                                        throw std::runtime_error(
                                            "source layout inheritance write failed; may be partial");
                                    out.call = LayoutReference(inherit_set, inherit_field, to.plugin, nullptr);
                                    if (!out.call.returned)
                                        throw std::runtime_error(
                                            "target layout inheritance write failed; may be partial");
                                    if (Pair(from, to, out)) {
                                        Write(to.plugin, enabled, true);
                                        Write(to.plugin, fade, command.fade);
                                        out.state = EffectControlState::Applied;
                                        out.detail = "layout transferred";
                                    }
                                }
                            }
                        }
                    }
                } else if (command.action == EffectLayoutAction::Perform) {
                    if (Current(from, out)) {
                        const auto index = static_cast<int>(command.perform);
                        out.call = LayoutPerform(perform[index], from.driver, index == 3);
                        if (!out.call.returned)
                            throw std::runtime_error("layout perform failed; may be partial");
                        out.state = EffectControlState::Applied;
                        out.detail = "layout perform submitted";
                    }
                } else if (Current(from, out)) {
                    Write(from.plugin, hidden, command.action == EffectLayoutAction::Hide);
                    if (command.action == EffectLayoutAction::Show)
                        Write(from.plugin, fade, command.fade);
                    out.state = EffectControlState::Applied;
                    out.detail = "layout visibility input written";
                }
            }
        } catch (const std::exception& error) {
            out.state = EffectControlState::Failed;
            out.detail = error.what();
        }
        out.references_released = Release(temporary);
        return out;
    }
    bool Close() {
        if (busy)
            return false;
        closed = true;
        const bool a = Release(temporary), b = Release(types);
        return a && b;
    }
};
}
EffectLayoutServices BindEffectLayoutNative(EffectLayoutNativeBindings binding, EffectAccess access,
                                            EffectRelations relations, ReferenceMethods refs) {
    auto s = std::make_shared<NativeLayouts>(std::move(access), std::move(relations), refs);
    static_cast<EffectLayoutNativeBindings&>(*s) = std::move(binding);
    s->Keep(s->plugin_type, s->types);
    s->Keep(s->inherit_field, s->types);
    return {[s](auto ref, const auto& command) { return s->Execute(ref, command); }, [s] { return s->Close(); }};
}
std::shared_ptr<EffectLayouts> PrepareEffectLayouts(RuntimeMetadata& m, EffectAccess access, ReferenceMethods refs) {
    auto s = std::make_shared<NativeLayouts>(m, std::move(access), refs);
    try {

        static_cast<EffectLayoutNativeBindings&>(*s) =
            ResolveEffectLayoutBindings(m, [s](ObjectResult result) { return s->Keep(result); });
        return std::make_shared<EffectLayouts>(EffectLayoutServices{
            [s](auto ref, const auto& command) { return s->Execute(ref, command); }, [s] { return s->Close(); }});
    } catch (...) {
        s->Close();
        throw;
    }
}
}
