#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Host tests of HEN's code that does not need the console.
set -eu
cd "$(dirname "$0")"
nx=${LIBOUNCE:-../../../../api_reimplement/libounce-nx/nx}
mkdir -p build/tests
cxx=${CXX:-g++}
# libnx's HIPC parser reads unaligned fields, as allowed on AArch64. Keep the
# other ASan/UBSan checks.
flags="-std=gnu++20 -Wall -Wextra -Werror -Wno-missing-field-initializers -fsanitize=address,undefined -fno-sanitize=alignment -D__SWITCH__ -D__SWITCH2__ -DLIBNX_HOST_TEST_TLS"
$cxx $flags -I"$nx/include" -I../libkaihen/include tests/mount_request_test.cpp -o build/tests/mount_request
build/tests/mount_request
$cxx $flags -I"$nx/include" -I../libkaihen/include tests/sm_request_test.cpp -o build/tests/sm_request
build/tests/sm_request
