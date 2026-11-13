#include "EffectObservationBindings.hpp"
#include <stdexcept>
namespace hksr::api::detail {
namespace {
std::size_t Field(const std::vector<DeclaredField>& fields, const char* owner, const char* name, const char* type) {
    const DeclaredField* found = nullptr;
    for (const auto& f : fields)
        if (f.owner_namespace == "RPG.Client" && f.owner_name == owner && f.name == name) {
            if (found || f.type != type || !f.flags_observed || (f.flags & 0x10))
                throw std::invalid_argument("effect scope field declaration differs");
            found = &f;
        }
    if (!found)
        throw std::invalid_argument(std::string("effect scope field absent: ") + owner + "." + name);
    return found->offset;
}
std::size_t Field(const std::vector<DeclaredField>& fields, const char* ns, const char* owner, const char* name,
                  const char* type) {
    const DeclaredField* found = nullptr;
    for (const auto& f : fields)
        if (f.owner_namespace == ns && f.owner_name == owner && f.name == name) {
            if (found || f.type != type || !f.flags_observed || (f.flags & 0x10))
                throw std::invalid_argument(std::string("effect submission field differs: ") + owner + "." + name);
            found = &f;
        }
    if (!found)
        throw std::invalid_argument(std::string("effect submission field absent: ") + owner + "." + name);
    return found->offset;
}
}
EffectScopeBindings ResolveEffectScopeBindings(MethodResolver& m, const EffectObservationFields& fields) {
    EffectScopeBindings out;
    try {
        out.owner_mono = m.Resolve("RPG.Client", "MonoEffectBehavior", "get_OwnerMono", "RPG.Client.MonoEffect", {});
    } catch (const std::exception& e) {
        out.behavior_error = e.what();
    }
    try {
        out.camera_plugin = m.Resolve("", "JCNLLEPCBGI", "HPNDIIPEFLJ", "RPG.Client.MonoEffectPluginCtrlCamera", {});
    } catch (const std::exception& e) {
        out.camera_error = e.what();
    }
    try {
        out.plugin_owner = Field(fields("RPG.Client", "MonoEffectPluginBase"), "MonoEffectPluginBase", "_OwnerEffect",
                                 "RPG.Client.MonoEffect");
    } catch (const std::exception& e) {
        out.plugin_error = e.what();
    }
    out.request_offset =
        Field(fields("RPG.Client", "MonoEffect"), "MonoEffect", "TriggerParams", "RPG.Client.TriggerEffectParams");
    out.id_offset = Field(fields("RPG.Client", "TriggerEffectParams"), "TriggerEffectParams",
                          "<UniqueEffectID>k__BackingField", "System.UInt32");
    return out;
}
std::size_t EffectScopeEntryGroup(std::size_t logical) {
    if (logical == 4)
        return 1;
    if (logical == 5 || logical == 6)
        return 2;
    if (logical < 8)
        return 0;
    throw std::out_of_range("effect scope logical entry");
}
EffectScopeEntries ResolveEffectScopeEntries(MethodResolver& m) {
    EffectScopeEntries out;
    const auto resolve = [&](std::size_t i) {
        switch (i) {
        case 0:
            return m.Resolve("RPG.Client", "MonoEffectBehavior", "PreResetPlugins", "System.Void", {});
        case 1:
            return m.Resolve("RPG.Client", "MonoEffectBehavior", "ResetPlugins", "System.Void", {});
        case 2:
            return m.Resolve("RPG.Client", "MonoEffectBehavior", "DisposePlugins", "System.Void", {});
        case 3:
            return m.Resolve("RPG.Client", "MonoEffectBehavior", "Tick", "System.Void", {"System.Single"});
        case 4:
            return m.Resolve("RPG.Client", "MonoEffectFixedCRPPlugin", "EFPBCKGBFBG", "System.Void", {});
        case 5:
            return m.Resolve("", "JCNLLEPCBGI", "NMIPDBNGLBH", "System.Void", {"System.Boolean"});
        case 6:
            return m.Resolve("", "JCNLLEPCBGI", "BLOMJCEJLFG", "System.Void", {});
        case 7:
            return m.Resolve("RPG.Client", "MonoEffectBehavior", "SetVisible", "System.Void", {"System.Boolean"});
        }
        throw std::out_of_range("effect scope entry");
    };
    for (std::size_t group = 0; group < out.supported.size(); ++group) {
        try {
            for (std::size_t logical = 0; logical < out.methods.size(); ++logical)
                if (EffectScopeEntryGroup(logical) == group)
                    out.methods[logical] = resolve(logical);
            out.supported[group] = true;
        } catch (const std::exception& e) {
            for (std::size_t logical = 0; logical < out.methods.size(); ++logical)
                if (EffectScopeEntryGroup(logical) == group)
                    out.methods[logical] = {};
            out.errors.push_back(e.what());
        }
    }
    return out;
}
EffectSubmissionBindings ResolveEffectSubmissionBindings(MethodResolver& methods,
                                                         const EffectObservationFields& fields) {
    EffectSubmissionBindings out;
    const auto request = fields("RPG.Client", "TriggerEffectParams");
    out.id_offset =
        Field(request, "RPG.Client", "TriggerEffectParams", "<UniqueEffectID>k__BackingField", "System.UInt32");
    out.caster_offset = Field(request, "RPG.Client", "TriggerEffectParams", "CasterEntity", "RPG.GameCore.GameEntity");
    try {
        out.caster = methods.Resolve("RPG.GameCore", "TaskContext", "get_CasterEntity", "RPG.GameCore.GameEntity", {});
        const auto task = fields("", "NFGENDDOCJJ");
        out.config_offset = Field(task, "", "NFGENDDOCJJ", "OFKGLJOAMLD", "RPG.GameCore.TriggerEffect");
        out.context_offset = Field(task, "", "NFGENDDOCJJ", "DDDNLILNMKA", "RPG.GameCore.TaskContext");
    } catch (const std::exception& e) {
        out.trigger_error = e.what();
    }
    return out;
}
EffectSubmissionEntries ResolveEffectSubmissionEntries(MethodResolver& methods) {
    EffectSubmissionEntries out;
    const auto bind = [&](std::size_t i, auto resolve) {
        try {
            out.methods[i] = resolve().code;
        } catch (const std::exception& e) {
            out.errors.push_back(e.what());
        }
    };
    bind(0, [&] {
        return methods.Resolve("RPG.Client", "MonoEffectManager", "FireEffectAsync", "System.Void",
                               {"RPG.Client.TriggerEffectParams"});
    });
    bind(1, [&] {
        return methods.Resolve("RPG.Client", "MonoEffectManager", "FireEffect", "RPG.Client.MonoEffect",
                               {"RPG.Client.TriggerEffectParams"});
    });
    bind(2, [&] { return methods.Resolve("", "NFGENDDOCJJ", "OnTaskBegin", "System.Void", {}); });
    return out;
}
}
