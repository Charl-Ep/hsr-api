#pragma once
#include <hksr/Export.hpp>
#include <string>

namespace hksr::api {
// Worker-thread file read. Opaque installed-data identity for OfficialCatalog's
// ReadCatalogRevision; consumers do not interpret its contents.
HKSR_API std::string ReadInstalledCatalogRevision();
}
