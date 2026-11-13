#pragma once
#include <memory>
#include "References.hpp"
#include <string>
#include <vector>
#include <utility>

namespace hksr::api {
struct ArrayMethods {
    Method create, length, get, set;
    std::vector<std::string> binding_errors;
};
// Rank-one arrays, official reflection access (including the GC write barrier),
// not raw writes into managed memory. Type/array/value inputs must be rooted.
class ManagedArrays {
  public:
    HKSR_API explicit ManagedArrays(ArrayMethods methods);
    HKSR_API ~ManagedArrays();
    HKSR_API ManagedArrays(const ManagedArrays&) noexcept;
    HKSR_API ManagedArrays& operator=(const ManagedArrays&) noexcept;
    HKSR_API ObjectResult Create(void* element_system_type, std::int32_t count) const;
    HKSR_API Observed<std::int32_t> Length(void* array) const;
    HKSR_API ObjectResult Get(void* array, std::int32_t index) const;
    HKSR_API CallResult Set(void* array, void* value, std::int32_t index) const;

  private:
    std::shared_ptr<const ArrayMethods> methods_;
};
HKSR_API ArrayMethods ResolveArrayMethods(MethodResolver& source);
}
