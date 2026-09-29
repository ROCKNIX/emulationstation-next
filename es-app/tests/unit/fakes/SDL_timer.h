// A stand-in for SDL's timer header, for es-conf-tests only: SystemConf.cpp
// times one debug line with SDL_GetTicks, and the unit tests build with the
// host compiler and no SDL (README.md beside CMakeLists.txt).
#pragma once

#include <cstdint>

inline uint32_t SDL_GetTicks() { return 0; }
