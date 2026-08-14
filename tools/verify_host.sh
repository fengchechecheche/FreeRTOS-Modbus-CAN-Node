#!/usr/bin/env sh
set -eu

python3 tools/verify_modbus_contract.py

for preset in host-debug host-release; do
    cmake --preset "${preset}"
    cmake --build --preset "${preset}"
    ctest --preset "${preset}" --output-on-failure
done
