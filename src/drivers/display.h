#pragma once

#include <M5Cardputer.h>

namespace pd {

class Display {
public:
    Display();

    bool begin();
    void beginFrame();
    void endFrame();
    M5Canvas& canvas() { return canvas_; }

private:
    M5Canvas canvas_;
};

}  // namespace pd

