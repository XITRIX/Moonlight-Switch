// SPDX-License-Identifier: GPL-2.0-only
#pragma once
extern "C" {
#include <switch/result.h>
}

// Result helpers in the style of libstratosphere's results_common.hpp, for
// code whose failure handling is to abort the operation and return. Where a
// failure path must also release or roll back, keep an explicit
// `if (R_FAILED(rc))` instead.

// Returns from the enclosing function when a step fails, carrying its Result.
#define R_TRY(expr)                                                            \
    do {                                                                       \
        const Result _r_try_rc = (expr);                                       \
        if (R_FAILED(_r_try_rc)) return _r_try_rc;                             \
    } while (0)

// Returns res from the enclosing function unless the condition holds. Use one
// per precondition so each reads as its own guard clause.
#define R_UNLESS(expr, res)                                                    \
    do {                                                                       \
        if (!(expr)) return (res);                                             \
    } while (0)

// Returns success from the enclosing function.
#define R_SUCCEED() return 0
