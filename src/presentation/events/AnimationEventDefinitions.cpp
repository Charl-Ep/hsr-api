#include <hksr/presentation/events/AnimationEventDefinitions.hpp>
#include "../../../adapters/presentation/EventRecordSchema.hpp"
#include <hksr/managed/ManagedRoots.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <algorithm>
#include <stdexcept>

namespace hksr::api {
namespace {
class Reader {
    ObjectMetadata& metadata_;
    const ManagedArrays& arrays_;
    const ComponentConfig& text_;
    ManagedRoots roots_;
    std::vector<ManagedRootId> held_;

  public:
    Reader(ObjectMetadata& metadata, ReferenceMethods refs, const ManagedArrays& arrays, const ComponentConfig& text)
        : metadata_(metadata), arrays_(arrays), text_(text), roots_(refs) {}
    // Roots are normally temporary. An uncertain release is not retried by
    // destruction, matching ManagedRoots' owner-thread cleanup contract.
    bool Close() {
        bool complete = true;
        for (auto id : held_)
            if (roots_.Release(id).complete)
                roots_.Forget(id);
            else
                complete = false;
        held_.clear();
        return complete;
    }
    void* Keep(ObjectResult result) {
        if (!result.call.returned)
            throw std::runtime_error("event definition array read failed");
        if (!result.object)
            return nullptr;
        held_.push_back(0);
        held_.back() = roots_.Keep(result.object);
        if (!roots_.Status(held_.back()).ready)
            throw std::runtime_error("event definition retention failed");
        return result.object;
    }
    void* Field(void* object, const char* name, const char* type) {
        const auto fields = metadata_.ReadObjectFields(object);
        const DeclaredField* selected = nullptr;
        for (const auto& field : fields)
            if (field.name == name && field.type == type && field.flags_observed && !(field.flags & 0x10)) {
                if (selected)
                    throw std::runtime_error(std::string("ambiguous event field: ") + name);
                selected = &field;
            }
        void* value = nullptr;
        if (!selected ||
            !native::CopyMemoryChecked(&value, static_cast<unsigned char*>(object) + selected->offset, sizeof(value)))
            throw std::runtime_error(std::string("unread event field: ") + name);
        return value;
    }
    std::string Text(void* object, const char* name) {
        auto* value = Field(object, name, "System.String");
        if (!value)
            return {};
        const auto text = text_.ReadText(value);
        if (!text.read.returned)
            throw std::runtime_error("event definition text unread");
        return text.utf8;
    }
    int Count(void* array) {
        if (!array)
            return 0;
        const auto n = arrays_.Length(array);
        if (!n.read.returned || n.value < 0)
            throw std::runtime_error("event definition array length unread");
        return n.value;
    }
    void Type(void* object, const char* space, const char* expected) {
        const auto actual = metadata_.ReadObjectTypeName(object);
        if (actual.name_space != space || actual.name != expected)
            throw std::runtime_error("unexpected event definition type");
    }
    void* Item(void* array, int index) {
        return Keep(arrays_.Get(array, index));
    }
    void Read(void* array, AnimationEventPart part, AnimationEventDefinitions& output) {
        int index = -1;
        try {
            const auto n = Count(array);
            for (index = 0; index < n; ++index)
                try {
                    auto* object = Keep(arrays_.Get(array, index));
                    if (!object)
                        continue;
                    output.rows.push_back(detail::DecodeEventRecord(*this, object, part, index));
                } catch (const std::exception& error) {
                    output.issues.push_back({part, index, error.what()});
                }
        } catch (const std::exception& error) {
            output.issues.push_back({part, index, error.what()});
        }
    }
};
}
AnimationEventDefinitions ReadAnimationEventDefinitions(const AnimationEventSource& source, ObjectMetadata& metadata,
                                                        ReferenceMethods refs, const ManagedArrays& arrays,
                                                        const ComponentConfig& text) {
    auto reader = std::make_unique<Reader>(metadata, refs, arrays, text);
    AnimationEventDefinitions result;
    try {
        reader->Read(source.events, AnimationEventPart::TimePoint, result);
        reader->Read(source.groups, AnimationEventPart::GroupEnter, result);
        reader->Read(source.groups, AnimationEventPart::GroupExit, result);
        reader->Read(source.behaviors, AnimationEventPart::Behavior, result);
    } catch (...) {
        if (!reader->Close())
            reader.release();
        throw;
    }
    if (!reader->Close()) {
        reader.release();
        result.issues.push_back(
            {AnimationEventPart::TimePoint, -1, "event definition GC cleanup unresolved; receipt retained"});
    }
    return result;
}
}
