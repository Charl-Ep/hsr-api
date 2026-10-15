#pragma once
#include "Interface.hpp"
#if defined(HKSR_API_BUILD)
#define HKSR_API __declspec(dllexport)
#else
#define HKSR_API __declspec(dllimport)
#endif
