#include "TimelineRegistryAccess.hpp"
#include "../../../adapters/presentation/TimelineRuntimeBindings.hpp"
#include <hksr/native/MetadataReader.hpp>
#include <hksr/native/Invocation.hpp>
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/managed/ManagedLists.hpp>
#include <hksr/unity/ComponentConfig.hpp>
#include <set>

namespace hksr::api::detail {
namespace {
template <class T> T Value(void* object, std::size_t offset) {
    T value{};
    if (!object || !native::CopyMemoryChecked(&value, static_cast<unsigned char*>(object) + offset, sizeof(value)))
        throw std::runtime_error("Timeline declared field unreadable");
    return value;
}
}
struct TimelineRegistryAccess::State : TimelineRegistryBindings {
    ObjectMetadata& metadata;
    ManagedRoots roots;
    ComponentConfig strings;
    std::vector<ManagedRootId> held;
    std::set<void*> retained;
    bool closed = false;
    State(MethodResolver& methods, ObjectMetadata& d, ReferenceMethods refs, const Fields& fields)
        : metadata(d), roots(refs), strings(ResolveComponentConfigMethods(methods)) {
        static_cast<TimelineRegistryBindings&>(*this) = ResolveTimelineRegistryBindings(fields);
    }
    void Keep(void* object) {
        if (closed)
            throw std::logic_error("Timeline registry reader already closed");
        if (!object || retained.count(object))
            return;
        held.reserve(held.size() + 1);
        const auto handle = roots.Keep(object);
        held.push_back(handle);
        if (!roots.Status(handle).ready)
            throw std::runtime_error("Timeline registry GC retention failed");
        retained.insert(object);
    }
    void Check(void* object, TimelineObject role) {
        if (!object)
            throw std::runtime_error("null Timeline object");
        Keep(object);
        const auto actual = metadata.ReadObjectType(object);
        if (!IsTimelineObject(actual, role))
            throw std::runtime_error("Timeline object type differs");
    }
    void* Ref(void* object, std::size_t offset) {
        auto* value = Value<void*>(object, offset);
        Keep(value);
        return value;
    }
};
TimelineRegistryAccess::TimelineRegistryAccess(MethodResolver& m, ObjectMetadata& d, ReferenceMethods refs,
                                               Fields fields)
    : state_(std::make_unique<State>(m, d, refs, fields)) {}
TimelineRegistryAccess::~TimelineRegistryAccess() {
    // No engine calls during destruction. Preserve uncertain GC receipts rather
    // than retry a decrement or hide a failed cleanup. Normal users call Close.
    if (!state_->held.empty())
        state_.release();
}
void TimelineRegistryAccess::Hold(void* object) {
    state_->Keep(object);
}
void* TimelineRegistryAccess::Find(void* registry, void* director) {
    auto& s = *state_;
    s.Check(registry, TimelineObject::Registry);
    s.Keep(director);
    if (!director)
        return nullptr;
    auto* dictionary = s.Ref(registry, s.dictionary);
    if (!dictionary)
        throw std::runtime_error("Timeline Director dictionary absent");
    const auto find = ResolveTimelineDirectorLookup(s.metadata, dictionary);
    bool found = false;
    void* record = nullptr;
    if (!native::Call(find, &found, dictionary, director, &record).returned)
        throw std::runtime_error("Timeline Director dictionary lookup failed");
    if (!found)
        return nullptr;
    s.Check(record, TimelineObject::Record);
    if (s.Ref(record, s.director) != director)
        throw std::runtime_error("Timeline Director dictionary/record disagree");
    return record;
}
TimelineRegistryRecord TimelineRegistryAccess::Read(void* registry, void* record) {
    auto& s = *state_;
    s.Check(registry, TimelineObject::Registry);
    s.Check(record, TimelineObject::Record);
    TimelineRegistryRecord out;
    out.object = record;
    out.view.key = {reinterpret_cast<std::uintptr_t>(registry), reinterpret_cast<std::uintptr_t>(record)};
    out.view.playback = Value<std::int32_t>(record, s.id);
    out.view.phase = DecodeTimelinePhase(Value<std::int32_t>(record, s.phase));
    out.director = s.Ref(record, s.director);
    out.resource = s.Ref(record, s.resource);
    out.runtime_name = s.Ref(record, s.name);
    out.resource_is_null = !out.resource;
    if (out.resource) {
        const auto text = s.strings.ReadText(out.resource);
        if (!text.read.returned)
            throw std::runtime_error("Timeline resource text unreadable: " + text.error);
        out.view.resource = text.utf8;
    }
    return out;
}
TimelineRegistryRows TimelineRegistryAccess::Rows(void* registry) {
    auto& s = *state_;
    TimelineRegistryRows out;
    try {
        s.Check(registry, TimelineObject::Registry);
        auto* list = s.Ref(registry, s.list);
        if (!list)
            throw std::runtime_error("Timeline record list absent");
        ManagedReferenceList access(ResolveTimelineRecordList(s.metadata.ReadObjectType(list)));
        const auto count = access.Count(list);
        if (!count.read.returned || count.value < 0)
            throw std::runtime_error("Timeline record count unreadable");
        for (std::int32_t i = 0; i < count.value; ++i) {
            try {
                const auto item = access.Item(list, i);
                if (!item.call.returned)
                    throw std::runtime_error("Timeline record list item unreadable");
                out.records.push_back(Read(registry, item.object));
            } catch (const std::exception& e) {
                out.complete = false;
                out.errors.push_back("row " + std::to_string(i) + ": " + e.what());
            }
        }
    } catch (const std::exception& e) {
        out.complete = false;
        out.errors.push_back(e.what());
    }
    return out;
}
bool TimelineRegistryAccess::Close() {
    auto& s = *state_;
    s.closed = true;
    for (auto i = s.held.begin(); i != s.held.end();) {
        if (s.roots.Release(*i).complete) {
            s.roots.Forget(*i);
            i = s.held.erase(i);
        } else
            ++i;
    }
    return s.held.empty();
}
}
