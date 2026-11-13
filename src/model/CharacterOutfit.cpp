#include <hksr/model/CharacterOutfit.hpp>
#include "../../adapters/model/OutfitBindings.hpp"
#include <hksr/runtime/OperationTrace.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/managed/ManagedArrays.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>
#include <algorithm>

namespace hksr::api {
using namespace detail;
struct CharacterOutfit::State {
    MethodResolver& methods;
    ObjectMetadata& metadata;
    ManagedObjects& objects;
    ManagedRoots roots;
    EntityAccess entities;
    CharacterOutfitStatus status;
    std::vector<ManagedRootId> handles;
    void* component = nullptr;
    void* original = nullptr;
    Method replace, refresh;
    bool changed = false, refresh_attempted = false;
    State(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, ReferenceMethods r, EntityAccessMethods e)
        : methods(m), metadata(d), objects(o), roots(r), entities(std::move(e)) {}
    void* Keep(ObjectResult value, bool nullable = false) {
        if (!value.call.returned || (!nullable && !value.object))
            throw std::runtime_error("outfit object unavailable");
        if (!value.object)
            return nullptr;
        handles.push_back(0);
        handles.back() = roots.Keep(value.object);
        if (!roots.Status(handles.back()).ready)
            throw std::runtime_error("outfit object retention failed");
        return value.object;
    }
    template <class... Args> ObjectResult Object(Method method, Args... args) {
        ObjectResult out;
        out.call = native::Call(method, &out.object, args...);
        return out;
    }
    using Model = OutfitModel;
    std::vector<Model> Models(void* list) {
        ManagedReferenceList access(ResolveOutfitModelList(metadata, list));
        const auto count = access.Count(list);
        if (!count.read.returned || count.value < 0)
            throw std::runtime_error("outfit comparison list unread");
        std::vector<Model> out;
        for (int i = 0; i < count.value; ++i) {
            auto* model = Keep(access.Item(list, i));
            out.push_back(ReadOutfitModel(metadata, model));
        }
        return out;
    }
    bool SameConfig(void* a, void* b, ManagedArrays& arrays, Method equal) {
        if (a == b)
            return true;
        if (!a || !b)
            return false;
        const auto stringsEqual = [&](void* x, void* y, OutfitString name) {
            auto* left = ReadOutfitString(metadata, x, name);
            auto* right = ReadOutfitString(metadata, y, name);
            bool same = false;
            if (!native::Call(equal, &same, left, right).returned)
                throw std::runtime_error("outfit string comparison failed");
            return same;
        };
        // Compare declared content, not object addresses: GetModels may create
        // fresh config objects for an unchanged outfit.
        for (bool decorations : {false, true}) {
            auto* left = ReadOutfitConfigParts(metadata, a, decorations);
            auto* right = ReadOutfitConfigParts(metadata, b, decorations);
            if (left == right)
                continue;
            if (!left || !right)
                return false;
            const auto lc = arrays.Length(left), rc = arrays.Length(right);
            if (!lc.read.returned || !rc.read.returned || lc.value < 0 || rc.value < 0)
                throw std::runtime_error("outfit config array unread");
            if (lc.value != rc.value)
                return false;
            for (int i = 0; i < lc.value; ++i) {
                auto* x = Keep(arrays.Get(left, i), true);
                auto* y = Keep(arrays.Get(right, i), true);
                if (x == y)
                    continue;
                if (!x || !y)
                    return false;
                if (decorations) {
                    if (!stringsEqual(x, y, OutfitString::Prefab))
                        return false;
                } else if (ReadOutfitStatic(metadata, x) != ReadOutfitStatic(metadata, y) ||
                           !stringsEqual(x, y, OutfitString::Material) || !stringsEqual(x, y, OutfitString::Mesh))
                    return false;
            }
        }
        return true;
    }
    bool MissesConfigChange(void* before, void* after) {
        const auto old = Models(before), next = Models(after);
        if (old.size() != next.size())
            return false;
        // Match the official membership test, without assuming list ordering.
        for (const auto& x : old)
            if (std::none_of(next.begin(), next.end(), [&](const Model& y) { return x.id == y.id; }))
                return false;
        for (const auto& x : next)
            if (std::none_of(old.begin(), old.end(), [&](const Model& y) { return x.id == y.id; }))
                return false;
        ArrayMethods arrayMethods;
        arrayMethods.length = methods.Resolve("System", "Array", "get_Length", "System.Int32", {});
        arrayMethods.get = methods.Resolve("System", "Array", "GetValue", "System.Object", {"System.Int32"});
        ManagedArrays arrays(arrayMethods);
        const auto equal = methods.Resolve("System", "String", "op_Equality", "System.Boolean",
                                           {"System.String", "System.String"}, MethodKind::Static);
        for (const auto& x : next) {
            bool same = false;
            for (const auto& y : old)
                if (x.id == y.id && SameConfig(x.config, y.config, arrays, equal)) {
                    same = true;
                    break;
                }
            if (!same)
                return true;
        }
        return false;
    }
};
CharacterOutfit::CharacterOutfit(MethodResolver& m, ObjectMetadata& d, ManagedObjects& o, ReferenceMethods r,
                                 EntityAccessMethods e)
    : state_(std::make_unique<State>(m, d, o, r, std::move(e))) {}
CharacterOutfit::~CharacterOutfit() = default; // no engine calls
const CharacterOutfitStatus& CharacterOutfit::Status() const {
    return state_->status;
}
void CharacterOutfit::Prepare(GameEntity* entity, std::uint32_t avatar) {
    OperationSpan timing("sdk.outfit.prepare");
    auto& s = *state_;
    auto& status = s.status;
    if (status.started || status.closed)
        throw std::logic_error("outfit override is single-use");
    status.started = true;
    status.avatar = avatar;
    try {
        if (!entity || !avatar)
            throw std::invalid_argument("outfit needs explicit owned entity and Avatar ID");
        const auto hero = ResolveOutfitApplicability(s.methods);
        if (!native::Call(hero, &status.applicable, avatar).returned)
            throw std::runtime_error("official outfit applicability read failed");
        if (!status.applicable)
            return; // Other characters do not need this assembly layer.
        auto* type = s.Keep(ResolveOutfitComponentType(s.objects));
        s.component = s.Keep(s.entities.Component(entity, type));
        s.original = ReadOutfitOverride(s.metadata, s.component);
        s.Keep({{true, 0}, s.original}, true);
        s.replace = ResolveOutfitReplace(s.methods);
        const auto selection =
            SelectOutfitModels(s.methods, s.metadata, avatar, [&](ObjectResult value) { return s.Keep(value, true); });
        status.gender = selection.gender;
        status.base_type = selection.base_type;
        status.errors.insert(status.errors.end(), selection.errors.begin(), selection.errors.end());
        auto* selected = s.Keep(selection.models);
        try {
            const auto sourceModels = ResolveOutfitSourceModels(s.methods);
            auto* before = s.Keep(s.Object(sourceModels, s.component));
            status.config_refresh_required = s.MissesConfigChange(before, selected);
        } catch (const std::exception& e) {
            // An unread comparison is diagnostic, not a new gate on the
            // already supported official override. Never force an unknown reload.
            status.errors.push_back(std::string("outfit refresh comparison incomplete: ") + e.what());
        }
        // The official method may assign before throwing. Retain the original
        // for teardown even when there is no successful return.
        s.changed = true;
        status.apply = native::CallVoid(s.replace, s.component, selected, false);
        status.applied = status.apply.returned;
        if (!status.applied)
            throw std::runtime_error("official instance outfit override failed");
        if (status.config_refresh_required) {
            s.refresh = ResolveOutfitRefresh(s.methods);
            s.refresh_attempted = true; // A throwing refresh may already replace parts.
            status.refresh = native::CallVoid(s.refresh, s.component);
            if (!status.refresh.returned)
                throw std::runtime_error("official outfit config refresh failed");
        }
    } catch (const std::exception& e) {
        status.errors.push_back(e.what());
    }
}
bool CharacterOutfit::Close(bool owner_alive) {
    auto& s = *state_;
    auto& status = s.status;
    if (status.closed)
        return true;
    if (s.changed && owner_alive && !status.restore_attempted) {
        status.restore_attempted = true;
        status.restore = native::CallVoid(s.replace, s.component, s.original, false);
        if (!status.restore.returned)
            status.errors.push_back("official outfit restoration failed; not retried");
        else if (s.refresh_attempted && !native::CallVoid(s.refresh, s.component).returned)
            status.errors.push_back("official outfit restoration config refresh failed; not retried");
    }
    // No repeat application/restoration and no global write during teardown.
    for (auto it = s.handles.begin(); it != s.handles.end();) {
        if (!*it || s.roots.Release(*it).complete) {
            if (*it)
                s.roots.Forget(*it);
            it = s.handles.erase(it);
        } else
            ++it;
    }
    status.closed = s.handles.empty();
    return status.closed;
}
}
