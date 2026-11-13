#include <hksr/presentation/effects/EffectLayouts.hpp>
#include "../../../src/presentation/effects/EffectLayoutNative.hpp"
#include <Windows.h>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace hksr::api;
namespace {
void Check(bool value) {
    if (!value)
        throw std::runtime_error("layout native contract");
}
struct Effect;
struct Plugin;
struct Params {
    std::uint32_t id;
    bool released = false;
    void* caster = nullptr;
    void* target = nullptr;
    void* param = nullptr;
    Effect* instance = nullptr;
};
struct Driver {
    Plugin* owner = nullptr;
};
struct Plugin {
    Effect* owner = nullptr;
    Driver* driver = nullptr;
    bool enabled = true, hidden = false, fade = false;
    Plugin* inherit = nullptr;
    int items = 0, pending = 0;
};
struct Effect {
    Params* request = nullptr;
    Plugin* plugin = nullptr;
};
std::map<int, void*> roots;
int serial = 0, kind = 0, field = 0, info = 0, receiver = 0;
unsigned queries = 0, performs = 0, copies = 0, clears = 0, references = 0;
bool copy_fault = false, absent = false, id_fault = false;
std::function<void()> component_hook;
bool Kept(void* value) {
    for (const auto& [id, p] : roots) {
        (void)id;
        if (p == value)
            return true;
    }
    return false;
}
template <class T> Method M(T function) {
    return {reinterpret_cast<void*>(function), &info};
}
int __fastcall Keep(void* object, int, int, void*) {
    roots[++serial] = object;
    return serial;
}
void* __fastcall Target(int id, void*) {
    return roots.at(id);
}
void __fastcall Free(int id, void*) {
    Check(roots.erase(id) == 1);
}
unsigned __fastcall Id(Params* p, void*) {
    if (id_fault)
        RaiseException(0xe0720001, 0, 0, nullptr);
    return p->id;
}
bool __fastcall Released(Params* p, void*) {
    return p->released;
}
void* __fastcall Instance(Params* p, void*) {
    return p->instance;
}
bool __fastcall Alive(void* p, void*) {
    Check(Kept(p));
    return true;
}
void* __fastcall Component(Effect* effect, void* type, void*) {
    Check(Kept(effect) && Kept(type) && type == &kind);
    ++queries;
    if (component_hook)
        component_hook();
    return absent ? nullptr : effect->plugin;
}
void __fastcall Perform0(Driver* driver, void*) {
    Check(Kept(driver));
    ++performs;
    driver->owner->items += driver->owner->pending;
    driver->owner->pending = 0;
}
void __fastcall Perform1(Driver* driver, void*) {
    Check(Kept(driver));
    ++performs;
    driver->owner->items = 0;
}
void __fastcall Perform2(Driver* driver, void*) {
    Check(Kept(driver));
    ++performs;
}
void __fastcall Perform3(Driver* driver, bool flag, void*) {
    Check(Kept(driver) && !flag);
    ++performs;
}
void __fastcall Copy(Driver* target, Plugin* source, void*) {
    Check(Kept(target) && Kept(source) && !source->enabled);
    ++copies;
    target->owner->items = source->items;
    target->owner->pending = source->pending;
    if (copy_fault)
        RaiseException(0xe0720002, 0, 0, nullptr);
}
void __fastcall Clear(Driver* source, void*) {
    Check(Kept(source));
    ++clears;
    source->owner->items = 0;
    source->owner->pending = 0;
}
void __fastcall Reference(void* field_object, Plugin* plugin, Plugin* value, int flags, void* binder, void* culture,
                          void*) {
    Check(field_object == &field && Kept(field_object) && Kept(plugin) && (!value || Kept(value)) && flags == 0 &&
          !binder && !culture);
    ++references;
    plugin->inherit = value;
}
struct ActorEffect {
    Params params;
    Effect effect;
    Plugin plugin;
    Driver driver;
    explicit ActorEffect(unsigned id) : params{id} {
        params.target = &receiver;
        params.instance = &effect;
        effect.request = &params;
        effect.plugin = &plugin;
        plugin.owner = &effect;
        plugin.driver = &driver;
        driver.owner = &plugin;
    }
    EffectReference Ref() {
        return {params.id, 0, &params};
    }
};
std::vector<DeclaredField> Fields() {
    using F = DeclaredField;
    return {F{"RPG.Client", "TriggerEffectParams", "CasterEntity", "RPG.GameCore.GameEntity", offsetof(Params, caster),
              6, true},
            F{"RPG.Client", "TriggerEffectParams", "TargetEntity", "RPG.GameCore.GameEntity", offsetof(Params, target),
              6, true},
            F{"RPG.Client", "TriggerEffectParams", "ParamEntity", "RPG.GameCore.GameEntity", offsetof(Params, param), 6,
              true},
            F{"RPG.Client", "TriggerEffectParams", "<EffectInstance>k__BackingField", "RPG.Client.MonoEffect",
              offsetof(Params, instance), 6, true},
            F{"RPG.Client", "TriggerEffectParams", "<UniqueEffectID>k__BackingField", "System.UInt32",
              offsetof(Params, id), 6, true},
            F{"RPG.Client", "TriggerEffectParams", "<IsReleased>k__BackingField", "System.Boolean",
              offsetof(Params, released), 6, true}};
}
std::unique_ptr<EffectLayouts> Create() {
    EffectAccessMethods access;
    access.id = M(Id);
    access.released = M(Released);
    access.instance = M(Instance);
    detail::EffectLayoutNativeBindings b;
    b.component = M(Component);
    b.alive = M(Alive);
    b.perform[0] = M(Perform0);
    b.perform[1] = M(Perform1);
    b.perform[2] = M(Perform2);
    b.perform[3] = M(Perform3);
    b.copy = M(Copy);
    b.clear = M(Clear);
    b.inherit_set = M(Reference);
    b.plugin_type = &kind;
    b.inherit_field = &field;
    b.trigger = offsetof(Effect, request);
    b.plugin_owner = offsetof(Plugin, owner);
    b.behavior = offsetof(Plugin, driver);
    b.behavior_owner = offsetof(Driver, owner);
    b.enabled = offsetof(Plugin, enabled);
    b.hidden = offsetof(Plugin, hidden);
    b.fade = offsetof(Plugin, fade);
    return std::make_unique<EffectLayouts>(detail::BindEffectLayoutNative(
        b, EffectAccess(access), EffectRelations(Fields()), ReferenceMethods{M(Keep), M(Target), M(Free), {}, {}}));
}
}
int main() try {
    auto api = Create();
    ActorEffect source(3), target(4);
    EffectLayoutCommand command;
    const auto a = source.Ref(), b = target.Ref();
    Check(roots.size() == 2);
    command.action = EffectLayoutAction::Hide;
    Check(api->Execute(a, command).state == EffectControlState::Applied && source.plugin.hidden && !source.plugin.fade);
    command.action = EffectLayoutAction::Show;
    command.fade = true;
    Check(api->Execute(a, command).state == EffectControlState::Applied && !source.plugin.hidden && source.plugin.fade);
    source.plugin.pending = 1;
    command.action = EffectLayoutAction::Perform;
    for (int i = 0; i < 4; ++i) {
        command.perform = static_cast<EffectLayoutPerform>(i);
        Check(api->Execute(a, command).state == EffectControlState::Applied);
    }
    Check(performs == 4 && roots.size() == 2);
    source.plugin.items = 2;
    source.plugin.pending = 1;
    command.action = EffectLayoutAction::Transfer;
    command.target = b;
    command.fade = false;
    Check(api->Execute(a, command).state == EffectControlState::Applied && !source.plugin.enabled &&
          target.plugin.enabled && source.plugin.inherit == &target.plugin && !target.plugin.inherit);
    Check(source.plugin.items == 0 && source.plugin.pending == 0 && target.plugin.items == 2 &&
          target.plugin.pending == 1 && copies == 1 && clears == 1 && references == 2 && roots.size() == 2);
    Check(api->Execute(a, command).state == EffectControlState::NoMatch && copies == 1);
    command.target = a;
    Check(api->Execute(a, command).state == EffectControlState::Failed && copies == 1);
    command.target = b;
    source.plugin.enabled = true;
    target.params.target = &kind;
    Check(api->Execute(a, command).state == EffectControlState::Unsupported && source.plugin.enabled && copies == 1);
    target.params.target = &receiver;
    source.plugin.items = 7;
    copy_fault = true;
    const auto failed = api->Execute(a, command);
    copy_fault = false;
    Check(failed.state == EffectControlState::Failed && failed.references_released && !source.plugin.enabled &&
          source.plugin.items == 7 && target.plugin.items == 7 && clears == 1 && references == 2 &&
          !source.params.released && !target.params.released && roots.size() == 2);
    command.action = EffectLayoutAction::Hide;
    absent = true;
    Check(api->Execute(a, command).state == EffectControlState::Unsupported);
    absent = false;
    source.params.instance = nullptr;
    const auto before = queries;
    Check(api->Execute(a, command).state == EffectControlState::NotLoaded && queries == before);
    source.params.instance = &source.effect;
    const auto old = a;
    component_hook = [&] { ++source.params.id; };
    Check(api->Execute(old, command).state == EffectControlState::Ended);
    component_hook = {};
    id_fault = true;
    Check(api->Execute(source.Ref(), command).state == EffectControlState::Failed);
    id_fault = false;
    Check(api->Close() && roots.empty() && api->Execute(source.Ref(), command).state == EffectControlState::Ended);
    std::cout
        << "Effect layout native receipt identity, four performs, transfer ABI, partial failure and roots passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
