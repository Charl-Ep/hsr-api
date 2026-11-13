#pragma once
#include <hksr/presentation/attachments/AttachmentVisibility.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/managed/ManagedArrays.hpp>
namespace hksr::api::detail {
std::unique_ptr<AttachmentVisibility> NativeAttachmentVisibility(MethodResolver&, ObjectMetadata&, ManagedObjects&,
                                                                 ReferenceMethods, ManagedArrays, void* component,
                                                                 std::function<bool()> current);
}
