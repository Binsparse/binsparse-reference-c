/*
 * SPDX-FileCopyrightText: 2026 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <binsparse/version.h>
#include <semver/semver.hpp>

#include <exception>

namespace {

bool bsp_semver_compatible(const semver::version& version,
                           const semver::version& supported) {
  // Mirror python-semver Version.is_compatible().
  if (supported.major() == 0 && version.major() == 0 &&
      (supported.minor() != version.minor() ||
       supported.patch() != version.patch() ||
       supported.prerelease() != version.prerelease())) {
    return false;
  }

  return supported.major() == version.major() &&
         version.minor() >= supported.minor() &&
         supported.prerelease() == version.prerelease();
}

} // namespace

extern "C" bsp_error_t bsp_check_version_compatible(const char* version) {
  if (version == nullptr) {
    return BSP_ERROR_FORMAT;
  }

  try {
    semver::version parsed = semver::version::parse(version);
    semver::version supported = semver::version::parse(BINSPARSE_VERSION);
    if (!bsp_semver_compatible(parsed, supported)) {
      return BSP_ERROR_UNSUPPORTED;
    }
  } catch (const std::exception&) {
    return BSP_ERROR_FORMAT;
  }

  return BSP_SUCCESS;
}
