#pragma once

#ifdef NATIVE_BUILD

#include <stdint.h>

namespace lightwaveos {
namespace actors {

class RendererActor {
public:
    const char* getEffectName(uint8_t) const { return ""; }
};

} // namespace actors
} // namespace lightwaveos

#endif
