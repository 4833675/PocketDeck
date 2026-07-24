#pragma once

#include "core/physical_key.h"

namespace pd {

class TextKeymap {
public:
    static char character(PhysicalKey key, bool shift);
};

}  // namespace pd
