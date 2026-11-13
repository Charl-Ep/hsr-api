#pragma once
#include "../Export.hpp"

namespace hksr::api {
struct Position3 {
    float x = 0, y = 0, z = 0;
};
struct Rotation4 {
    float x = 0, y = 0, z = 0, w = 1;
};
static_assert(sizeof(Position3) == 12 && sizeof(Rotation4) == 16);
}
