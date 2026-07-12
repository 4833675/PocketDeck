#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
mkdir -p .build

sources=(test/native/test_main.cpp src/core/mac_keymap.cpp)
for source in src/core/input_router.cpp src/core/g0_gesture.cpp src/core/system_settings.cpp; do
    if [[ -f "$source" ]]; then
        sources+=("$source")
    fi
done

c++ -std=c++17 -Wall -Wextra -Werror -Isrc -Itest/native \
    "${sources[@]}" -o .build/native_tests

.build/native_tests
