#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

partition_range() {
    awk -F',' -v wanted="$1" '
        function trim(value) {
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", value)
            return value
        }
        $0 !~ /^[[:space:]]*#/ && trim($1) == wanted {
            print trim($4), trim($5)
            found = 1
            exit
        }
        END { if (!found) exit 1 }
    ' partitions_8mb.csv
}

read -r nvs_offset nvs_size < <(partition_range nvs)
read -r ota_offset ota_size < <(partition_range otadata)
boot_app0_start=$((0xe000))
boot_app0_end=$((0x10000))
if (( nvs_offset + nvs_size > boot_app0_start ||
      ota_offset > boot_app0_start || ota_offset + ota_size < boot_app0_end )); then
    echo "FAIL: partitions_8mb.csv overlaps or fails to reserve boot_app0 at 0xE000-0xFFFF" >&2
    exit 1
fi

mkdir -p .build

sources=(test/native/test_main.cpp src/core/mac_keymap.cpp src/core/text_keymap.cpp \
         src/core/lora_data.cpp src/core/lora_tx_policy.cpp src/core/media_data.cpp \
         src/core/motion_data.cpp)
for source in src/core/input_router.cpp src/core/g0_gesture.cpp src/core/system_settings.cpp \
              src/core/ble_keyboard_policy.cpp src/core/clock_data.cpp src/core/gps_data.cpp \
              src/apps/gps/gps_app_model.cpp src/apps/gps/gps_app_text.cpp \
              src/apps/keyboard/keyboard_app_text.cpp \
              src/apps/lora/lora_app_model.cpp \
              src/apps/lora/lora_app_text.cpp \
              src/apps/media/media_app_model.cpp \
              src/apps/media/media_app_text.cpp \
              src/apps/settings/settings_app_text.cpp \
              src/apps/ssh/ssh_app_text.cpp \
              src/core/serial_command.cpp \
              src/core/ssh_error_detail.cpp \
              src/core/ssh_retry_policy.cpp \
              src/core/ssh_hosts.cpp \
              src/core/ssh_host_record.cpp \
              src/core/terminal_buffer.cpp \
              src/core/terminal_input.cpp \
              src/core/wifi_data.cpp \
              src/core/wifi_recovery_policy.cpp \
              src/core/wifi_profiles.cpp \
              src/core/weather_data.cpp \
              src/apps/weather/weather_app_text.cpp \
              src/apps/launcher/launcher_model.cpp \
              src/apps/ssh/ssh_app_model.cpp \
              src/apps/settings/settings_model.cpp src/services/diagnostics_service.cpp \
              src/ui/quick_settings_model.cpp; do
    if [[ -f "$source" ]]; then
        sources+=("$source")
    fi
done

c++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Isrc -Itest/native \
    "${sources[@]}" -o .build/native_tests

.build/native_tests
