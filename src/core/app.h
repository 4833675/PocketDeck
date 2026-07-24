#pragma once

#include <cstdint>

#include "core/app_id.h"
#include "core/input.h"

namespace pd {

class Display;
struct SystemContext;

class App {
public:
    virtual ~App() = default;
    virtual AppId id() const = 0;
    virtual const char* title() const = 0;
    virtual InputMode inputMode() const { return InputMode::System; }
    virtual void onEnter(SystemContext& context) = 0;
    virtual void onExit(SystemContext& context) = 0;
    virtual void onInput(const InputEvent& event, SystemContext& context) = 0;
    virtual void update(uint32_t nowMs, SystemContext& context) = 0;
    virtual void render(Display& display, const SystemContext& context) = 0;
};

}  // namespace pd
