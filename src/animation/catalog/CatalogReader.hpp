#pragma once
#include "../../../adapters/animation/catalog/CatalogSchema.hpp"

namespace hksr::api {
struct OfficialCatalog::State : detail::CatalogSchema {
    using CatalogSchema::CatalogSchema;
    struct AbilityRead;
};
}
