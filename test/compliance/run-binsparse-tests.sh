#!/usr/bin/env bash

# SPDX-FileCopyrightText: 2026 Binsparse Developers
# SPDX-License-Identifier: BSD-3-Clause

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/../.." && pwd)"
build_dir="${BINSPARSE_BUILD_DIR:-${repo_root}/build-compliance}"
tests_dir="${BINSPARSE_TESTS_DIR:-${build_dir}/binsparse-tests}"
tests_ref="${BINSPARSE_TESTS_REF:-9df16f6e7fe6d73c147ea5a3c23cf203876386b6}"
build_config="${BINSPARSE_BUILD_CONFIG:-Release}"

for command in git pixi; do
  if ! command -v "${command}" >/dev/null 2>&1; then
    echo "error: required command '${command}' was not found" >&2
    exit 1
  fi
done

if [[ -d "${tests_dir}/.git" ]]; then
  git -C "${tests_dir}" fetch --depth 1 origin "${tests_ref}"
  git -C "${tests_dir}" checkout --detach FETCH_HEAD
elif [[ -e "${tests_dir}" ]]; then
  echo "error: ${tests_dir} exists but is not a Git checkout" >&2
  exit 1
else
  git clone --depth 1 --branch "${tests_ref}" \
    https://github.com/Binsparse/binsparse-tests.git "${tests_dir}"
fi

(
  cd "${tests_dir}"
  pixi install -e test
)

executable_dir="${build_dir}/test/compliance"
if [[ -d "${executable_dir}/${build_config}" ]]; then
  executable_dir="${executable_dir}/${build_config}"
fi

for executable in npy_to_binsparse binsparse_to_npy binsparse_to_binsparse; do
  if [[ ! -x "${executable_dir}/${executable}" ]]; then
    echo "error: missing executable ${executable_dir}/${executable}" >&2
    exit 1
  fi
done

export BINSPARSE_BIN="${executable_dir}"
(
  cd "${tests_dir}"
  pixi run -e test pytest -m hdf5 binsparse_tests/ "$@"
)
