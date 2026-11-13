#pragma once
#include <hksr/metadata/RuntimeMetadata.hpp>
#include "../../src/metadata/InstanceField.hpp"
#include <hksr/native/Invocation.hpp>
#include <hksr/native/MetadataReader.hpp>
#include <stdexcept>

namespace hksr::api::detail {
template <class T> T ReadAttachmentField(ObjectMetadata& metadata, void* object, const char* name, const char* type) {
    const auto field = ResolveInstanceField(metadata, object, name, type);
    T value{};
    if (!native::CopyMemoryChecked(&value, static_cast<char*>(object) + field.offset, sizeof(value)))
        throw std::runtime_error(std::string("attachment field unread: ") + name);
    return value;
}
}
