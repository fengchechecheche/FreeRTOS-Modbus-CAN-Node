#!/usr/bin/env sh
set -eu

python3 tools/check_shell_line_endings.py
python3 tools/verify_modbus_contract.py
python3 tools/soak_runner.py --self-test
python3 tools/soak_hil_collector.py --self-test

for preset in host-debug host-release; do
    cmake --preset "${preset}"
    cmake --build --preset "${preset}"
    ctest --preset "${preset}" --output-on-failure
done
