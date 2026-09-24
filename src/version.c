/*
 * SPDX-FileCopyrightText: 2026 Binsparse Developers
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <binsparse/version.h>

#include <semver.h>

#include <stdbool.h>

static bool parse_version(const char* text, semver_t* version) {
  return text != NULL && semver_parse(text, version) == 0;
}

static bool compatible(const semver_t* version, const semver_t* supported) {
  if (version->major != supported->major)
    return false;

  if (supported->major == 0)
    return semver_compare_version(*version, *supported) == 0;

  return version->minor >= supported->minor;
}

bsp_error_t bsp_check_version_compatible(const char* version) {
  semver_t parsed = {0};
  semver_t supported = {0};
  bsp_error_t error = BSP_SUCCESS;
  bool parsed_ok = parse_version(version, &parsed);
  bool supported_ok = parsed_ok && parse_version(BINSPARSE_VERSION, &supported);

  if (!parsed_ok || !supported_ok) {
    semver_free(&parsed);
    semver_free(&supported);
    return BSP_ERROR_FORMAT;
  }

  if (!compatible(&parsed, &supported))
    error = BSP_ERROR_UNSUPPORTED;

  semver_free(&parsed);
  semver_free(&supported);
  return error;
}
