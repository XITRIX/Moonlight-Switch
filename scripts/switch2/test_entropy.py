#!/usr/bin/env python3
"""Host mock checks for the SDK entropy adapter's failure and cleanup contract."""

import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    cc = shutil.which("cc") or shutil.which("gcc")
    if not cc:
        raise SystemExit("A host C compiler is required for the entropy adapter test")
    with tempfile.TemporaryDirectory(prefix="moonlight-entropy-") as name:
        tmp = Path(name)
        (tmp / "mbedtls").mkdir()
        (tmp / "switch.h").write_text("""#include <stddef.h>
typedef unsigned int Result;
#define R_FAILED(rc) ((rc) != 0)
Result csrngInitialize(void);
Result csrngGetRandomBytes(void*, size_t);
void csrngExit(void);
""")
        (tmp / "mbedtls/entropy.h").write_text(
            "#define MBEDTLS_ERR_ENTROPY_SOURCE_FAILED -60\n"
        )
        (tmp / "mbedtls/entropy_poll.h").write_text("""#include <stddef.h>
int mbedtls_hardware_poll(void*, unsigned char*, size_t, size_t*);
""")
        (tmp / "test.c").write_text("""#include <assert.h>
#include <string.h>
#include "switch.h"
#include "mbedtls/entropy.h"
#include "mbedtls/entropy_poll.h"
static unsigned int init_error, read_error;
static int inits, reads, exits;
Result csrngInitialize(void) { ++inits; return init_error; }
Result csrngGetRandomBytes(void* out, size_t len) {
    ++reads;
    if (!read_error) memset(out, 0xa5, len);
    return read_error;
}
void csrngExit(void) { ++exits; }
int main(void) {
    unsigned char out[32] = {0};
    size_t written = 123;
    init_error = 1;
    assert(mbedtls_hardware_poll(0, out, sizeof out, &written) == MBEDTLS_ERR_ENTROPY_SOURCE_FAILED);
    assert(written == 0 && inits == 1 && reads == 0 && exits == 0 && out[0] == 0);
    init_error = 0; read_error = 2; written = 123;
    assert(mbedtls_hardware_poll(0, out, sizeof out, &written) == MBEDTLS_ERR_ENTROPY_SOURCE_FAILED);
    assert(written == 0 && reads == 1 && exits == 1 && out[0] == 0);
    read_error = 0;
    assert(mbedtls_hardware_poll(0, out, sizeof out, &written) == 0);
    assert(written == sizeof out && reads == 2 && exits == 2);
    for (size_t i = 0; i < sizeof out; ++i) assert(out[i] == 0xa5);
    written = 123;
    assert(mbedtls_hardware_poll(0, out, 0, &written) == 0);
    assert(written == 0 && reads == 3 && exits == 3);
    return 0;
}
""")
        exe = tmp / "entropy-test.exe"
        subprocess.run(
            [
                cc,
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(tmp),
                str(ROOT / "scripts/switch2/mbedtls_entropy.c"),
                str(tmp / "test.c"),
                "-o",
                str(exe),
            ],
            check=True,
        )
        subprocess.run([str(exe)], check=True)
    print(
        "Entropy adapter host mock: init failure, read failure, success and cleanup passed"
    )


if __name__ == "__main__":
    main()
