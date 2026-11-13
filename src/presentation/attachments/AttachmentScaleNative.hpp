#pragma once
#include <hksr/presentation/attachments/AttachmentScale.hpp>
#include <hksr/metadata/RuntimeMetadata.hpp>
#include <hksr/managed/ManagedObjects.hpp>
namespace hksr::api::detail {
std::unique_ptr<AttachmentScale> NativeAttachmentScale(MethodResolver&, ObjectMetadata&, ManagedObjects&,
                                                       ReferenceMethods, void* model, std::function<bool()> current);
}
