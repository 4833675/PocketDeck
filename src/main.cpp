#include "core/system.h"

namespace {
pd::System pocketDeckSystem;
}

void setup() { pocketDeckSystem.begin(); }
void loop() { pocketDeckSystem.update(); }
