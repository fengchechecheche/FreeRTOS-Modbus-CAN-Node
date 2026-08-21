#!/usr/bin/env bash

set -euo pipefail

readonly expected_commit="d13b6db511d76885533c6e7e0ef22d74a5e2d817"
readonly script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly repo_root="$(cd -- "${script_dir}/../.." && pwd)"
readonly board_patch="${script_dir}/0000-adapter-led-routing.patch"
readonly one_shot_patch="${script_dir}/0001-advertise-one-shot.patch"

usage() {
  printf 'Usage: %s <candleLight_fw-source> [output-directory]\n' "$0" >&2
  printf 'The output directory must not already exist. No device is flashed.\n' >&2
}

if (( $# < 1 || $# > 2 )); then
  usage
  exit 2
fi

readonly source_dir="$(realpath -- "$1")"
readonly output_dir="${2:-${repo_root}/out/candlelight-one-shot}"
readonly source_copy="${output_dir}/source"
readonly build_dir="${output_dir}/build"

if [[ ! -d "${source_dir}/.git" ]]; then
  printf 'ERROR: not a candleLight_fw Git checkout: %s\n' "${source_dir}" >&2
  exit 2
fi

readonly actual_commit="$(git -C "${source_dir}" rev-parse HEAD)"
if [[ "${actual_commit}" != "${expected_commit}" ]]; then
  printf 'ERROR: expected candleLight_fw commit %s, got %s\n' \
    "${expected_commit}" "${actual_commit}" >&2
  exit 2
fi

if [[ -e "${output_dir}" ]]; then
  printf 'ERROR: output directory already exists: %s\n' "${output_dir}" >&2
  exit 2
fi

mkdir -p -- "${output_dir}"
git clone --local --no-hardlinks --no-checkout "${source_dir}" "${source_copy}"
git -C "${source_copy}" config core.filemode false
git -C "${source_copy}" checkout --detach "${expected_commit}"
git -C "${source_copy}" apply --check "${board_patch}" "${one_shot_patch}"
git -C "${source_copy}" apply "${board_patch}" "${one_shot_patch}"

cmake \
  -S "${source_copy}" \
  -B "${build_dir}" \
  -DCMAKE_TOOLCHAIN_FILE="${source_copy}/cmake/gcc-arm-none-eabi-8-2019-q3-update.cmake" \
  -DBUILD_candleLight=ON \
  -DBUILD_cantact=OFF \
  -DBUILD_canalyze=OFF \
  -DBUILD_canable=OFF \
  -DBUILD_usb2can=OFF

cmake --build "${build_dir}" --target candleLight_fw --parallel 2

test -s "${build_dir}/candleLight_fw.bin"
readonly firmware_hash="$(sha256sum "${build_dir}/candleLight_fw.bin")"
printf '%s  candleLight_fw.bin\n' "${firmware_hash%% *}" > "${output_dir}/SHA256SUMS.txt"

printf 'PATCHED_COMMIT=%s\n' "${expected_commit}"
printf 'FIRMWARE=%s\n' "${build_dir}/candleLight_fw.bin"
printf 'SHA256=%s\n' "${firmware_hash%% *}"
printf 'FLASHED=NO\n'
