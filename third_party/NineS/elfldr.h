/* Copyright (C) 2024 John Törnblom
 *
 * Derived from elfldr (https://github.com/ps5-payload-dev/elfldr) by John
 * Törnblom, licensed under the GNU General Public License v3 or later; local
 * changes for OrangeHUD. ELF load helpers for the inject path.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Load ELF image into target process; returns entry VA or 0 on failure. */
intptr_t elfldr_load(pid_t pid, uint8_t *elf);

/** Allocate payload_args_t-compatible block in target. Returns VA or 0. */
intptr_t elfldr_payload_args(pid_t pid);

#ifdef __cplusplus
}
#endif
