#pragma once

#include "core/ir_data.h"

namespace pd {

class IrService {
public:
    void setActive(bool active);
    bool active() const;
    bool send(SonyIrCommand command);

private:
    bool active_ = false;
};

}  // namespace pd
