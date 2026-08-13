#!/usr/bin/env sh
set -eu

printf 'git: '; git --version
printf 'gcc: '; gcc --version | sed -n '1p'
printf 'cmake: '; cmake --version | sed -n '1p'
printf 'ctest: '; ctest --version | sed -n '1p'
printf 'ninja: '; ninja --version
printf 'arm_gcc: '; arm-none-eabi-gcc --version | sed -n '1p'
printf 'python: '; python3 --version
printf 'clang_format: '; clang-format --version
printf 'clang_tidy: '; clang-tidy --version | sed -n '1p'
printf 'cppcheck: '; cppcheck --version
