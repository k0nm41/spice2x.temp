#pragma once

#include <cstdint>

#include <windows.h>

namespace mouseoverride {

    bool available();
    void write(int32_t x, int32_t y, bool pressed);
    void write_reset();
    bool read(POINT *position, bool *pressed);
    bool canvas(SIZE *size);
}
