#!/usr/bin/env sh
set -eu

for preset in host-debug host-release; do
    cmake --preset "${preset}"
    cmake --build --preset "${preset}"
    ctest --preset "${preset}" --output-on-failure
done
